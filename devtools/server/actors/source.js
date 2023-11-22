/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const { sourceSpec } = require("resource://devtools/shared/specs/source.js");

const {
  setBreakpointAtEntryPoints,
} = require("resource://devtools/server/actors/breakpoint.js");
const {
  getSourcemapBaseURL,
} = require("resource://devtools/server/actors/utils/source-map-utils.js");
const {
  getDebuggerSourceURL,
} = require("resource://devtools/server/actors/utils/source-url.js");

loader.lazyRequireGetter(
  this,
  "ArrayBufferActor",
  "resource://devtools/server/actors/array-buffer.js",
  true
);
loader.lazyRequireGetter(
  this,
  "LongStringActor",
  "resource://devtools/server/actors/string.js",
  true
);

loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "resource://devtools/shared/DevToolsUtils.js"
);

const windowsDrive = /^([a-zA-Z]:)/;

function resolveSourceURL(sourceURL, targetActor) {
  if (sourceURL) {
    try {
      let baseURL;
      if (targetActor.window) {
        baseURL = targetActor.window.location?.href;
      }
      // For worker, we don't have easy access to location,
      // so pull extra information directly from the target actor.
      if (targetActor.workerUrl) {
        baseURL = targetActor.workerUrl;
      }
      return new URL(sourceURL, baseURL || undefined).href;
    } catch (err) {}
  }

  return null;
}
function getSourceURL(source, targetActor) {
  // Some eval sources have URLs, but we want to explicitly ignore those because
  // they are generally useless strings like "eval" or "debugger eval code".
  let resourceURL = getDebuggerSourceURL(source) || "";

  // Strip out eventual stack trace stored in Source's url.
  // (not clear if that still happens)
  resourceURL = resourceURL.split(" -> ").pop();

  // Debugger.Source.url attribute may be of the form:
  //   "http://example.com/foo line 10 > inlineScript"
  // because of the following function `js::FormatIntroducedFilename`:
  // https://searchfox.org/mozilla-central/rev/253ae246f642fe9619597f44de3b087f94e45a2d/js/src/vm/JSScript.cpp#1816-1846
  // This isn't so easy to reproduce, but browser_dbg-breakpoints-popup.js's testPausedInTwoPopups covers this
  resourceURL = resourceURL.replace(/ line \d+ > .*$/, "");

  // A "//# sourceURL=" pragma should basically be treated as a source file's
  // full URL, so that is what we want to use as the base if it is present.
  // If this is not an absolute URL, this will mean the maps in the file
  // will not have a valid base URL, but that is up to tooling that
  let result = resolveSourceURL(source.displayURL, targetActor);
  if (!result) {
    result = resolveSourceURL(resourceURL, targetActor) || resourceURL;

    // In XPCShell tests, the source URL isn't actually a URL, it's a file path.
    // That causes issues because "C:/folder/file.js" is parsed as a URL with
    // "c:" as the URL scheme, which causes the drive letter to be unexpectedly
    // lower-cased when the parsed URL is re-serialized. To avoid that, we
    // detect that case and re-uppercase it again. This is a bit gross and
    // ideally it seems like XPCShell tests should use file:// URLs for files,
    // but alas they do not.
    if (
      resourceURL &&
      resourceURL.match(windowsDrive) &&
      result.slice(0, 2) == resourceURL.slice(0, 2).toLowerCase()
    ) {
      result = resourceURL.slice(0, 2) + result.slice(2);
    }
  }

  // Avoid returning empty string and return null if no URL is found
  return result || null;
}

/**
 * A SourceActor provides information about the source of a script. Source
 * actors are 1:1 with Debugger.Source objects.
 *
 * @param Debugger.Source source
 *        The source object we are representing.
 * @param ThreadActor thread
 *        The current thread actor.
 */
class SourceActor extends Actor {
  constructor({ source, thread }) {
    super(thread.conn, sourceSpec);

    this._threadActor = thread;
    this._url = undefined;
    this._source = source;
    this.__isInlineSource = undefined;
  }

  get _isInlineSource() {
    const source = this._source;
    if (this.__isInlineSource === undefined) {
      // If the source has a usable displayURL, the source is treated as not
      // inlined because it has its own URL.
      // Also consider sources loaded from <iframe srcdoc> as independant sources,
      // because we can't easily fetch the full html content of the srcdoc attribute.
      this.__isInlineSource =
        source.introductionType === "inlineScript" &&
        !resolveSourceURL(source.displayURL, this.threadActor._parent) &&
        !this.url.startsWith("about:srcdoc");
    }
    return this.__isInlineSource;
  }

  get threadActor() {
    return this._threadActor;
  }
  get sourcesManager() {
    return this._threadActor.sourcesManager;
  }
  get dbg() {
    return this.threadActor.dbg;
  }
  get breakpointActorMap() {
    return this.threadActor.breakpointActorMap;
  }
  get url() {
    if (this._url === undefined) {
      this._url = getSourceURL(this._source, this.threadActor._parent);
    }
    return this._url;
  }

  get extensionName() {
    if (this._extensionName === undefined) {
      this._extensionName = null;

      // Cu is not available for workers and so we are not able to get a
      // WebExtensionPolicy object
      if (!isWorker && this.url?.startsWith("moz-extension:")) {
        try {
          const extURI = Services.io.newURI(this.url);
          const policy = WebExtensionPolicy.getByURI(extURI);
          if (policy) {
            this._extensionName = policy.name;
          }
        } catch (e) {
          console.warn(`Failed to find extension name for ${this.url} : ${e}`);
        }
      }
    }

    return this._extensionName;
  }

  get internalSourceId() {
    return this._source.id;
  }

  form() {
    const source = this._source;

    let introductionType = source.introductionType;
    if (
      introductionType === "srcScript" ||
      introductionType === "inlineScript" ||
      introductionType === "injectedScript"
    ) {
      // These three used to be one single type, so here we combine them all
      // so that clients don't see any change in behavior.
      introductionType = "scriptElement";
    }

    // NOTE: Debugger.Source.prototype.startColumn is 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = source.introductionType === "wasm" ? 0 : 1;

    return {
      actor: this.actorID,
      extensionName: this.extensionName,
      url: this.url,
      isBlackBoxed: this.sourcesManager.isBlackBoxed(this.url),
      sourceMapBaseURL: getSourcemapBaseURL(
        this.url,
        this.threadActor._parent.window
      ),
      sourceMapURL: source.sourceMapURL,
      introductionType,
      isInlineSource: this._isInlineSource,
      sourceStartLine: source.startLine,
      sourceStartColumn: source.startColumn - columnBase,
      sourceLength: source.text?.length,
    };
  }

  destroy() {
    const parent = this.getParent();
    if (parent && parent.sourceActors) {
      delete parent.sourceActors[this.actorID];
    }
    super.destroy();
  }

  get _isWasm() {
    return this._source.introductionType === "wasm";
  }

  async _getSourceText() {
    if (this._isWasm) {
      const wasm = this._source.binary;
      const buffer = wasm.buffer;
      DevToolsUtils.assert(
        wasm.byteOffset === 0 && wasm.byteLength === buffer.byteLength,
        "Typed array from wasm source binary must cover entire buffer"
      );
      return {
        content: buffer,
        contentType: "text/wasm",
      };
    }

    // Use `source.text` if it exists, is not the "no source" string, and
    // the source isn't one that is inlined into some larger file.
    // It will be "no source" if the Debugger API wasn't able to load
    // the source because sources were discarded
    // (javascript.options.discardSystemSource == true).
    //
    // For inline source, we do something special and ignore individual source content.
    // Instead, each inline source will return the full HTML page content where
    // the inline source is (i.e. `<script> js source </script>`).
    //
    // When using srcdoc attribute on iframes:
    //   <iframe srcdoc="<script> js source </script>"></iframe>
    // The whole iframe source is going to be considered as an inline source because displayURL is null
    // and introductionType is inlineScript. But Debugger.Source.text is the only way
    // to retrieve the source content.
    if (this._source.text !== "[no source]" && !this._isInlineSource) {
      return {
        content: this.actualText(),
        contentType: "text/javascript",
      };
    }

    return this.sourcesManager.urlContents(
      this.url,
      /* partial */ false,
      /* canUseCache */ this._isInlineSource
    );
  }

  // Get the actual text of this source, padded so that line numbers will match
  // up with the source itself.
  actualText() {
    // If the source doesn't start at line 1, line numbers in the client will
    // not match up with those in the source. Pad the text with blank lines to
    // fix this. This can show up for sources associated with inline scripts
    // in HTML created via document.write() calls: the script's source line
    // number is relative to the start of the written HTML, but we show the
    // source's content by itself.
    const padding = this._source.startLine
      ? "\n".repeat(this._source.startLine - 1)
      : "";
    return padding + this._source.text;
  }

  // Return whether the specified fetched contents includes the actual text of
  // this source in the expected position.
  contentMatches(fileContents) {
    const lineBreak = /\r\n?|\n|\u2028|\u2029/;
    const contentLines = fileContents.content.split(lineBreak);
    const sourceLines = this._source.text.split(lineBreak);
    let line = this._source.startLine - 1;
    for (const sourceLine of sourceLines) {
      const contentLine = contentLines[line++] || "";
      if (!contentLine.includes(sourceLine)) {
        return false;
      }
    }
    return true;
  }

  getBreakableLines() {
    const positions = this._getBreakpointPositions();
    const lines = new Set();
    for (const position of positions) {
      if (!lines.has(position.line)) {
        lines.add(position.line);
      }
    }

    return Array.from(lines);
  }

  // Get all toplevel scripts in the source. Transitive child scripts must be
  // found by traversing the child script tree.
  _getTopLevelDebuggeeScripts() {
    if (this._scripts) {
      return this._scripts;
    }

    let scripts = this.dbg.findScripts({ source: this._source });

    if (!this._isWasm) {
      // There is no easier way to get the top-level scripts right now, so
      // we have to build that up the list manually.
      // Note: It is not valid to simply look for scripts where
      // `.isFunction == false` because a source may have executed multiple
      // where some have been GCed and some have not (bug 1627712).
      const allScripts = new Set(scripts);
      for (const script of allScripts) {
        for (const child of script.getChildScripts()) {
          allScripts.delete(child);
        }
      }
      scripts = [...allScripts];
    }

    this._scripts = scripts;
    return scripts;
  }

  resetDebuggeeScripts() {
    this._scripts = null;
  }

  // Get toplevel scripts which contain all breakpoint positions for the source.
  // This is different from _scripts if we detected that some scripts have been
  // GC'ed and reparsed the source contents.
  _getTopLevelBreakpointPositionScripts() {
    if (this._breakpointPositionScripts) {
      return this._breakpointPositionScripts;
    }

    let scripts = this._getTopLevelDebuggeeScripts();

    // We need to find all breakpoint positions, even if scripts associated with
    // this source have been GC'ed. We detect this by looking for a script which
    // does not have a function: a source will typically have a top level
    // non-function script. If this top level script still exists, then it keeps
    // all its child scripts alive and we will find all breakpoint positions by
    // scanning the existing scripts. If the top level script has been GC'ed
    // then we won't find its breakpoint positions, and inner functions may have
    // been GC'ed as well. In this case we reparse the source and generate a new
    // and complete set of scripts to look for the breakpoint positions.
    // Note that in some cases like "new Function(stuff)" there might not be a
    // top level non-function script, but if there is a non-function script then
    // it must be at the top level and will keep all other scripts in the source
    // alive.
    if (!this._isWasm && !scripts.some(script => !script.isFunction)) {
      let newScript;
      try {
        newScript = this._source.reparse();
      } catch (e) {
        // reparse() will throw if the source is not valid JS. This can happen
        // if this source is the resurrection of a GC'ed source and there are
        // parse errors in the refetched contents.
      }
      if (newScript) {
        scripts = [newScript];
      }
    }

    this._breakpointPositionScripts = scripts;
    return scripts;
  }

  // Get all scripts in this source that might include content in the range
  // specified by the given query.
  _findDebuggeeScripts(query, forBreakpointPositions) {
    const scripts = forBreakpointPositions
      ? this._getTopLevelBreakpointPositionScripts()
      : this._getTopLevelDebuggeeScripts();

    const {
      start: { line: startLine = 0, column: startColumn = 0 } = {},
      end: { line: endLine = Infinity, column: endColumn = Infinity } = {},
    } = query || {};

    const rv = [];
    addMatchingScripts(scripts);
    return rv;

    function scriptMatches(script) {
      // These tests are approximate, as we can't easily get the script's end
      // column.
      let lineCount;
      try {
        lineCount = script.lineCount;
      } catch (err) {
        // Accessing scripts which were optimized out during parsing can throw
        // an exception. Tolerate these so that we can still get positions for
        // other scripts in the source.
        return false;
      }

      // NOTE: Debugger.Script.prototype.startColumn is 1-based.
      //       Convert to 0-based, while keeping the wasm's column (1) as is.
      //       (bug 1863878)
      const columnBase = script.format === "wasm" ? 0 : 1;
      if (
        script.startLine > endLine ||
        script.startLine + lineCount <= startLine ||
        (script.startLine == endLine &&
          script.startColumn - columnBase > endColumn)
      ) {
        return false;
      }

      if (
        lineCount == 1 &&
        script.startLine == startLine &&
        script.startColumn - columnBase + script.sourceLength <= startColumn
      ) {
        return false;
      }

      return true;
    }

    function addMatchingScripts(childScripts) {
      for (const script of childScripts) {
        if (scriptMatches(script)) {
          rv.push(script);
          if (script.format === "js") {
            addMatchingScripts(script.getChildScripts());
          }
        }
      }
    }
  }

  _getBreakpointPositions(query) {
    const scripts = this._findDebuggeeScripts(
      query,
      /* forBreakpointPositions */ true
    );

    const positions = [];
    for (const script of scripts) {
      this._addScriptBreakpointPositions(query, script, positions);
    }

    return (
      positions
        // Sort the items by location.
        .sort((a, b) => {
          const lineDiff = a.line - b.line;
          return lineDiff === 0 ? a.column - b.column : lineDiff;
        })
    );
  }

  _addScriptBreakpointPositions(query, script, positions) {
    const {
      start: { line: startLine = 0, column: startColumn = 0 } = {},
      end: { line: endLine = Infinity, column: endColumn = Infinity } = {},
    } = query || {};

    // NOTE: Debugger.Script.prototype.startColumn is 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;

    const offsets = script.getPossibleBreakpoints();
    for (const { lineNumber, columnNumber } of offsets) {
      if (
        lineNumber < startLine ||
        (lineNumber === startLine && columnNumber - columnBase < startColumn) ||
        lineNumber > endLine ||
        (lineNumber === endLine && columnNumber - columnBase >= endColumn)
      ) {
        continue;
      }

      positions.push({
        line: lineNumber,
        column: columnNumber - columnBase,
      });
    }
  }

  getBreakpointPositionsCompressed(query) {
    const items = this._getBreakpointPositions(query);
    const compressed = {};
    for (const { line, column } of items) {
      if (!compressed[line]) {
        compressed[line] = [];
      }
      compressed[line].push(column);
    }
    return compressed;
  }

  /**
   * Handler for the "onSource" packet.
   * @return Object
   *         The return of this function contains a field `contentType`, and
   *         a field `source`. `source` can either be an ArrayBuffer or
   *         a LongString.
   */
  async source() {
    try {
      const { content, contentType } = await this._getSourceText();
      if (
        typeof content === "object" &&
        content &&
        content.constructor &&
        content.constructor.name === "ArrayBuffer"
      ) {
        return {
          source: new ArrayBufferActor(this.threadActor.conn, content),
          contentType,
        };
      }

      return {
        source: new LongStringActor(this.threadActor.conn, content),
        contentType,
      };
    } catch (error) {
      throw new Error(
        "Could not load the source for " +
          this.url +
          ".\n" +
          DevToolsUtils.safeErrorString(error)
      );
    }
  }

  /**
   * Handler for the "blackbox" packet.
   */
  blackbox(range) {
    this.sourcesManager.blackBox(this.url, range);
    if (
      this.threadActor.state == "paused" &&
      this.threadActor.youngestFrame &&
      this.threadActor.youngestFrame.script.url == this.url
    ) {
      return true;
    }
    return false;
  }

  /**
   * Handler for the "unblackbox" packet.
   */
  unblackbox(range) {
    this.sourcesManager.unblackBox(this.url, range);
  }

  /**
   * Handler for the "setPausePoints" packet.
   *
   * @param Array pausePoints
   *        A dictionary of pausePoint objects
   *
   *        type PausePoints = {
   *          line: {
   *            column: { break?: boolean, step?: boolean }
   *          }
   *        }
   */
  setPausePoints(pausePoints) {
    const uncompressed = {};
    const points = {
      0: {},
      1: { break: true },
      2: { step: true },
      3: { break: true, step: true },
    };

    for (const line in pausePoints) {
      uncompressed[line] = {};
      for (const col in pausePoints[line]) {
        uncompressed[line][col] = points[pausePoints[line][col]];
      }
    }

    this.pausePoints = uncompressed;
  }

  /*
   * Ensure the given BreakpointActor is set as a breakpoint handler on all
   * scripts that match its location in the generated source.
   *
   * @param BreakpointActor actor
   *        The BreakpointActor to be set as a breakpoint handler.
   *
   * @returns A Promise that resolves to the given BreakpointActor.
   */
  async applyBreakpoint(actor) {
    const { line, column } = actor.location;

    // Find all entry points that correspond to the given location.
    const entryPoints = [];
    if (column === undefined) {
      // Find all scripts that match the given source actor and line
      // number.
      const query = { start: { line }, end: { line } };
      const scripts = this._findDebuggeeScripts(query).filter(
        script => !actor.hasScript(script)
      );

      // NOTE: Debugger.Script.prototype.getPossibleBreakpoints returns
      //       columnNumber in 1-based.
      //       The following code uses columnNumber only for comparing against
      //       other columnNumber, and we don't need to convert to 0-based.

      // This is a line breakpoint, so we add a breakpoint on the first
      // breakpoint on the line.
      const lineMatches = [];
      for (const script of scripts) {
        const possibleBreakpoints = script.getPossibleBreakpoints({ line });
        for (const possibleBreakpoint of possibleBreakpoints) {
          lineMatches.push({ ...possibleBreakpoint, script });
        }
      }
      lineMatches.sort((a, b) => a.columnNumber - b.columnNumber);

      if (lineMatches.length) {
        // A single Debugger.Source may have _multiple_ Debugger.Scripts
        // at the same position from multiple evaluations of the source,
        // so we explicitly want to take all of the matches for the matched
        // column number.
        const firstColumn = lineMatches[0].columnNumber;
        const firstColumnMatches = lineMatches.filter(
          m => m.columnNumber === firstColumn
        );

        for (const { script, offset } of firstColumnMatches) {
          entryPoints.push({ script, offsets: [offset] });
        }
      }
    } else {
      // Find all scripts that match the given source actor, line,
      // and column number.
      const query = { start: { line, column }, end: { line, column } };
      const scripts = this._findDebuggeeScripts(query).filter(
        script => !actor.hasScript(script)
      );

      for (const script of scripts) {
        // NOTE: getPossibleBreakpoints's minColumn/maxColumn parameters are
        //       1-based.
        //       Convert to 1-based, while keeping the wasm's column (1) as is.
        //       (bug 1863878)
        const columnBase = script.format === "wasm" ? 0 : 1;

        // Check to see if the script contains a breakpoint position at
        // this line and column.
        const possibleBreakpoint = script
          .getPossibleBreakpoints({
            line,
            minColumn: column + columnBase,
            maxColumn: column + columnBase + 1,
          })
          .pop();

        if (possibleBreakpoint) {
          const { offset } = possibleBreakpoint;
          entryPoints.push({ script, offsets: [offset] });
        }
      }
    }

    setBreakpointAtEntryPoints(actor, entryPoints);
  }
}

exports.SourceActor = SourceActor;
