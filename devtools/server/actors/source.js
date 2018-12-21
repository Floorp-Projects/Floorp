/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const { BreakpointActor, setBreakpointAtEntryPoints } = require("devtools/server/actors/breakpoint");
const { GeneratedLocation } = require("devtools/server/actors/common");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert, fetch } = DevToolsUtils;
const { joinURI } = require("devtools/shared/path");
const { sourceSpec } = require("devtools/shared/specs/source");
const { findClosestScriptBySource } = require("devtools/server/actors/utils/closest-scripts");

loader.lazyRequireGetter(this, "mapURIToAddonID", "devtools/server/actors/utils/map-uri-to-addon-id");
loader.lazyRequireGetter(this, "arrayBufferGrip", "devtools/server/actors/array-buffer", true);

function isEvalSource(source) {
  const introType = source.introductionType;
  // These are all the sources that are essentially eval-ed (either
  // by calling eval or passing a string to one of these functions).
  return (introType === "eval" ||
          introType === "debugger eval" ||
          introType === "Function" ||
          introType === "eventHandler" ||
          introType === "setTimeout" ||
          introType === "setInterval");
}

exports.isEvalSource = isEvalSource;

function getSourceURL(source, window) {
  if (isEvalSource(source)) {
    // Eval sources have no urls, but they might have a `displayURL`
    // created with the sourceURL pragma. If the introduction script
    // is a non-eval script, generate an full absolute URL relative to it.

    if (source.displayURL && source.introductionScript) {
      if (source.introductionScript.source.url === "debugger eval code") {
        if (window) {
          // If this is a named eval script created from the console, make it
          // relative to the current page. window is only available
          // when we care about this.
          return joinURI(window.location.href, source.displayURL);
        }
      } else if (!isEvalSource(source.introductionScript.source)) {
        return joinURI(source.introductionScript.source.url, source.displayURL);
      }
    }

    return source.displayURL;
  } else if (source.url === "debugger eval code") {
    // Treat code evaluated by the console as unnamed eval scripts
    return null;
  }
  return source.url;
}

exports.getSourceURL = getSourceURL;

/**
 * Resolve a URI back to physical file.
 *
 * Of course, this works only for URIs pointing to local resources.
 *
 * @param  uri
 *         URI to resolve
 * @return
 *         resolved nsIURI
 */
function resolveURIToLocalPath(uri) {
  let resolved;
  switch (uri.scheme) {
    case "jar":
    case "file":
      return uri;

    case "chrome":
      resolved = Cc["@mozilla.org/chrome/chrome-registry;1"]
                  .getService(Ci.nsIChromeRegistry).convertChromeURL(uri);
      return resolveURIToLocalPath(resolved);

    case "resource":
      resolved = Cc["@mozilla.org/network/protocol;1?name=resource"]
                  .getService(Ci.nsIResProtocolHandler).resolveURI(uri);
      uri = Services.io.newURI(resolved);
      return resolveURIToLocalPath(uri);

    default:
      return null;
  }
}

/**
 * A SourceActor provides information about the source of a script. There
 * are two kinds of source actors: ones that represent real source objects,
 * and ones that represent non-existant "original" sources when the real
 * sources are HTML documents. We separate these because there isn't a
 * 1:1 mapping of HTML to sources; one source may represent a subsection
 * of an HTML source, so we need to create N + 1 separate
 * actors.
 *
 * There are 2 different scenarios for sources that you should
 * understand:
 *
 * - A single source that is not inlined in HTML
 *   (separate JS file, eval'ed code, etc)
 * - An HTML page with multiple inline scripts, which are distinct
 *   sources, but should be represented as a single source
 *
 * The complexity of `SourceActor` and `ThreadSources` are to handle
 * all of thise cases and hopefully internalize the complexities.
 *
 * @param Debugger.Source source
 *        The source object we are representing.
 * @param ThreadActor thread
 *        The current thread actor.
 * @param String originalUrl
 *        Optional. For HTML documents urls, the original url this is representing.
 * @param Boolean isInlineSource
 *        Optional. True if this is an inline source from a HTML or XUL page.
 * @param String contentType
 *        Optional. The content type of this source, if immediately available.
 */
const SourceActor = ActorClassWithSpec(sourceSpec, {
  typeName: "source",

  initialize: function({ source, thread, originalUrl,
                          isInlineSource, contentType }) {
    this._threadActor = thread;
    this._originalUrl = originalUrl;
    this._source = source;
    this._contentType = contentType;
    this._isInlineSource = isInlineSource;

    this.onSource = this.onSource.bind(this);
    this._getSourceText = this._getSourceText.bind(this);

    this._mapSourceToAddon();

    this._init = null;
  },

  get isInlineSource() {
    return this._isInlineSource;
  },

  get threadActor() {
    return this._threadActor;
  },
  get sources() {
    return this._threadActor.sources;
  },
  get dbg() {
    return this.threadActor.dbg;
  },
  get source() {
    return this._source;
  },
  get breakpointActorMap() {
    return this.threadActor.breakpointActorMap;
  },
  get url() {
    if (this.source) {
      return getSourceURL(this.source, this.threadActor._parent.window);
    }
    return this._originalUrl;
  },
  get addonID() {
    return this._addonID;
  },
  get addonPath() {
    return this._addonPath;
  },

  get isCacheEnabled() {
    if (this.threadActor._parent._getCacheDisabled) {
      return !this.threadActor._parent._getCacheDisabled();
    }
    return true;
  },

  form: function() {
    const source = this.source;
    // This might not have a source because we treat HTML pages with
    // inline scripts as a special SourceActor that doesn't have either.
    let introductionUrl = null;
    if (source && source.introductionScript) {
      introductionUrl = source.introductionScript.source.url;
    }

    return {
      actor: this.actorID,
      url: this.url ? this.url.split(" -> ").pop() : null,
      addonID: this._addonID,
      addonPath: this._addonPath,
      isBlackBoxed: this.threadActor.sources.isBlackBoxed(this.url),
      sourceMapURL: source ? source.sourceMapURL : null,
      introductionUrl: introductionUrl ? introductionUrl.split(" -> ").pop() : null,
      introductionType: source ? source.introductionType : null,
    };
  },

  destroy: function() {
    if (this.registeredPool && this.registeredPool.sourceActors) {
      delete this.registeredPool.sourceActors[this.actorID];
    }
  },

  _findDebuggeeScripts(query = null) {
    query = { ...query };
    assert(
      !("url" in query) && !("source" in query),
      "Debuggee source and URL are set automatically"
    );

    // For most cases, we have a real source to query for. The
    // only time we don't is for HTML pages. In that case we want
    // to query for scripts in an HTML page based on its URL, as
    // there could be several sources within an HTML page.
    if (this.source) {
      query.source = this.source;
    } else {
      query.url = this.url;
    }
    return this.dbg.findScripts(query);
  },

  _mapSourceToAddon: function() {
    let nsuri;
    try {
      nsuri = Services.io.newURI(this.url.split(" -> ").pop());
    } catch (e) {
      // We can't do anything with an invalid URI
      return;
    }

    const localURI = resolveURIToLocalPath(nsuri);
    if (!localURI) {
      return;
    }

    const id = mapURIToAddonID(localURI);
    if (!id) {
      return;
    }
    this._addonID = id;

    if (localURI instanceof Ci.nsIJARURI) {
      // The path in the add-on is easy for jar: uris
      this._addonPath = localURI.JAREntry;
    } else if (localURI instanceof Ci.nsIFileURL) {
      // For file: uris walk up to find the last directory that is part of the
      // add-on
      const target = localURI.file;
      let path = target.leafName;

      // We can assume that the directory containing the source file is part
      // of the add-on
      let root = target.parent;
      let file = root.parent;
      while (file && mapURIToAddonID(Services.io.newFileURI(file))) {
        path = root.leafName + "/" + path;
        root = file;
        file = file.parent;
      }

      if (!file) {
        const error = new Error("Could not find the root of the add-on for " + this.url);
        DevToolsUtils.reportException("SourceActor.prototype._mapSourceToAddon", error);
        return;
      }

      this._addonPath = path;
    }
  },

  _reportLoadSourceError: function(error) {
    try {
      DevToolsUtils.reportException("SourceActor", error);

      JSON.stringify(this.form(), null, 4).split(/\n/g)
        .forEach(line => console.error("\t", line));
    } catch (e) {
      // ignore
    }
  },

  _getSourceText: async function() {
    const toResolvedContent = t => ({
      content: t,
      contentType: this._contentType,
    });
    const isWasm = this.source && this.source.introductionType === "wasm";

    if (isWasm) {
      const wasm = this.source.binary;
      const buffer = wasm.buffer;
      assert(
        wasm.byteOffset === 0 && wasm.byteLength === buffer.byteLength,
        "Typed array from wasm source binary must cover entire buffer"
      );
      return toResolvedContent(buffer);
    }

    // If we are replaying then we can only use source saved during the
    // original recording. If we try to fetch it now it may have changed or
    // may no longer exist.
    if (this.dbg.replaying) {
      assert(!this._contentType);
      return this.dbg.replayingContent(this.url);
    }

    // Use `source.text` if it exists, is not the "no source" string, and
    // the content type of the source is JavaScript or it is synthesized
    // wasm. It will be "no source" if the Debugger API wasn't able to load
    // the source because sources were discarded
    // (javascript.options.discardSystemSource == true). Re-fetch non-JS
    // sources to get the contentType from the headers.
    if (this.source &&
        this.source.text !== "[no source]" &&
        this._contentType &&
        (this._contentType.includes("javascript") ||
          this._contentType === "text/wasm")) {
      return toResolvedContent(this.source.text);
    }

    // Only load the HTML page source from cache (which exists when
    // there are inline sources). Otherwise, we can't trust the
    // cache because we are most likely here because we are
    // fetching the original text for sourcemapped code, and the
    // page hasn't requested it before (if it has, it was a
    // previous debugging session).
    // Additionally, we should only try the cache if it is currently enabled
    // for the document.  Without this check, the cache may return stale data
    // that doesn't match the document shown in the browser.
    const loadFromCache = this.isInlineSource && this.isCacheEnabled;

    // Fetch the sources with the same principal as the original document
    const win = this.threadActor._parent.window;
    let principal, cacheKey;
    // On xpcshell, we don't have a window but a Sandbox
    if (!isWorker && win instanceof Ci.nsIDOMWindow) {
      const docShell = win.docShell;
      const channel = docShell.currentDocumentChannel;
      principal = channel.loadInfo.loadingPrincipal;

      // Retrieve the cacheKey in order to load POST requests from cache
      // Note that chrome:// URLs don't support this interface.
      if (loadFromCache &&
        docShell.currentDocumentChannel instanceof Ci.nsICacheInfoChannel) {
        cacheKey = docShell.currentDocumentChannel.cacheKey;
      }
    }

    const sourceFetched = fetch(this.url, {
      principal,
      cacheKey,
      loadFromCache,
    });

    // Record the contentType we just learned during fetching
    return sourceFetched
      .then(result => {
        this._contentType = result.contentType;
        return result;
      }, error => {
        this._reportLoadSourceError(error);
        throw error;
      });
  },

  /**
   * Get all executable lines from the current source
   * @return Array - Executable lines of the current script
   */
  getExecutableLines: async function() {
    const offsetsLines = new Set();
    for (const s of this._findDebuggeeScripts()) {
      for (const offset of s.getAllColumnOffsets()) {
        offsetsLines.add(offset.lineNumber);
      }
    }

    const lines = [...offsetsLines];
    lines.sort((a, b) => {
      return a - b;
    });
    return lines;
  },

  getBreakpointPositions(query) {
    const {
      start: {
        line: startLine = 0,
        column: startColumn = 0,
      } = {},
      end: {
        line: endLine = Infinity,
        column: endColumn = Infinity,
      } = {},
    } = query || {};

    let scripts;
    if (Number.isFinite(endLine)) {
      const found = new Set();
      for (let line = startLine; line <= endLine; line++) {
        for (const script of this._findDebuggeeScripts({ line })) {
          found.add(script);
        }
      }
      scripts = Array.from(found);
    } else {
      scripts = this._findDebuggeeScripts();
    }

    const positions = [];
    for (const script of scripts) {
      const offsets = script.getAllColumnOffsets();
      for (const { lineNumber, columnNumber } of offsets) {
        if (
          lineNumber < startLine ||
          (lineNumber === startLine && columnNumber < startColumn) ||
          lineNumber > endLine ||
          (lineNumber === endLine && columnNumber > endColumn)
        ) {
          continue;
        }

        positions.push({
          line: lineNumber,
          column: columnNumber,
        });
      }
    }

    return positions
      // Sort the items by location.
      .sort((a, b) => {
        const lineDiff = a.line - b.line;
        return lineDiff === 0 ? a.column - b.column : lineDiff;
      })
      // Filter out duplicate locations since they are useless in this context.
      .filter((item, i, arr) => (
        i === 0 ||
        item.line !== arr[i - 1].line ||
        item.column !== arr[i - 1].column
      ));
  },

  getBreakpointPositionsCompressed(query) {
    const items = this.getBreakpointPositions(query);
    const compressed = {};
    for (const { line, column } of items) {
      if (!compressed[line]) {
        compressed[line] = [];
      }
      compressed[line].push(column);
    }
    return compressed;
  },

  /**
   * Handler for the "source" packet.
   */
  onSource: function() {
    return Promise.resolve(this._init)
      .then(this._getSourceText)
      .then(({ content, contentType }) => {
        if (typeof content === "object" && content && content.constructor &&
            content.constructor.name === "ArrayBuffer") {
          return {
            source: arrayBufferGrip(content, this.threadActor.threadLifetimePool),
            contentType,
          };
        }
        return {
          source: createValueGrip(content, this.threadActor.threadLifetimePool,
            this.threadActor.objectGrip),
          contentType: contentType,
        };
      })
      .catch(error => {
        reportError(error, "Got an exception during SA_onSource: ");
        throw new Error("Could not load the source for " + this.url + ".\n" +
                        DevToolsUtils.safeErrorString(error));
      });
  },

  /**
   * Handler for the "blackbox" packet.
   */
  blackbox: function(range) {
    this.threadActor.sources.blackBox(this.url, range);
    if (this.threadActor.state == "paused"
        && this.threadActor.youngestFrame
        && this.threadActor.youngestFrame.script.url == this.url) {
      return true;
    }
    return false;
  },

  /**
   * Handler for the "unblackbox" packet.
   */
  unblackbox: function(range) {
    this.threadActor.sources.unblackBox(this.url, range);
  },

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
  setPausePoints: function(pausePoints) {
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
  },

  /**
   * Handle a request to set a breakpoint.
   *
   * @param Number line
   *        Line to break on.
   * @param Number column
   *        Column to break on.
   * @param String condition
   *        A condition which must be true for breakpoint to be hit.
   * @param Boolean noSliding
   *        If true, disables breakpoint sliding.
   *
   * @returns Promise
   *          A promise that resolves to a JSON object representing the
   *          response.
   */
  setBreakpoint: function(line, column, condition, noSliding) {
    const location = new GeneratedLocation(this, line, column);
    const actor = this._getOrCreateBreakpointActor(
      location,
      condition,
      noSliding
    );

    const response = {
      actor: actor.actorID,
      isPending: actor.isPending,
    };

    const actualLocation = actor.generatedLocation;
    if (!actualLocation.equals(location)) {
      response.actualLocation = actualLocation.toJSON();
    }

    return response;
  },

  /**
   * Get or create a BreakpointActor for the given location in the generated
   * source, and ensure it is set as a breakpoint handler on all scripts that
   * match the given location.
   *
   * @param GeneratedLocation generatedLocation
   *        A GeneratedLocation representing the location of the breakpoint in
   *        the generated source.
   * @param String condition
   *        A string that is evaluated whenever the breakpoint is hit. If the
   *        string evaluates to false, the breakpoint is ignored.
   * @param Boolean noSliding
   *        If true, disables breakpoint sliding.
   *
   * @returns BreakpointActor
   *          A BreakpointActor representing the breakpoint.
   */
  _getOrCreateBreakpointActor: function(generatedLocation, condition, noSliding) {
    let actor = this.breakpointActorMap.getActor(generatedLocation);
    if (!actor) {
      actor = new BreakpointActor(this.threadActor, generatedLocation);
      this.threadActor.threadLifetimePool.addActor(actor);
      this.breakpointActorMap.setActor(generatedLocation, actor);
    }

    actor.condition = condition;

    return this._setBreakpoint(actor, noSliding);
  },

  /*
   * Ensure the given BreakpointActor is set as a breakpoint handler on all
   * scripts that match its location in the generated source.
   *
   * If there are no scripts that match the location of the BreakpointActor,
   * we slide its location to the next closest line (for line breakpoints) or
   * column (for column breakpoint) that does.
   *
   * If breakpoint sliding fails, then either there are no scripts that contain
   * any code for the given location, or they were all garbage collected before
   * the debugger started running. We cannot distinguish between these two
   * cases, so we insert the BreakpointActor in the BreakpointActorMap as
   * a pending breakpoint. Whenever a new script is introduced, this method is
   * called again for each pending breakpoint.
   *
   * @param BreakpointActor actor
   *        The BreakpointActor to be set as a breakpoint handler.
   * @param Boolean noSliding
   *        If true, disables breakpoint sliding.
   *
   * @returns A Promise that resolves to the given BreakpointActor.
   */
  _setBreakpoint: function(actor, noSliding) {
    const { generatedLocation } = actor;
    const { generatedLine, generatedSourceActor } = generatedLocation;

    const isWasm = this.source && this.source.introductionType === "wasm";
    if (!this._setBreakpointAtGeneratedLocation(actor, generatedLocation) &&
        !noSliding &&
        !isWasm) {
      const scripts = this._findDebuggeeScripts({ line: generatedLine });

      // Never do breakpoint sliding for column breakpoints.
      // Additionally, never do breakpoint sliding if no scripts
      // exist on this line.
      //
      // Sliding can go horribly wrong if we always try to find the
      // next line with valid entry points in the entire file.
      // Scripts may be completely GCed and we never knew they
      // existed, so we end up sliding through whole functions to
      // the user's bewilderment.
      //
      // We can slide reliably if any scripts exist, however, due
      // to how scripts are kept alive. A parent Debugger.Script
      // keeps all of its children alive, so as long as we have a
      // valid script, we can slide through it and know we won't
      // slide through any of its child scripts. Additionally, if a
      // script gets GCed, that means that all parents scripts are
      // GCed as well, and no scripts will exist on those lines
      // anymore. We will never slide through a GCed script.
      if (generatedLocation.generatedColumn || scripts.length === 0) {
        return actor;
      }

      // Find the script that spans the largest amount of code to
      // determine the bounds for sliding.
      const largestScript = scripts.reduce((largestScr, script) => {
        if (script.lineCount > largestScr.lineCount) {
          return script;
        }
        return largestScr;
      });
      const maxLine = largestScript.startLine + largestScript.lineCount - 1;

      let actualLine = generatedLine;
      for (; actualLine <= maxLine; actualLine++) {
        const loc = new GeneratedLocation(this, actualLine);
        if (this._setBreakpointAtGeneratedLocation(actor, loc)) {
          break;
        }
      }

      // The above loop should never complete. We only did breakpoint sliding
      // because we found scripts on the line we started from,
      // which means there must be valid entry points somewhere
      // within those scripts.
      if (actualLine > maxLine) {
        // eslint-disable-next-line no-throw-literal
        throw {
          error: "noCodeAtLineColumn",
          message:
            "Could not find any entry points to set a breakpoint on, " +
            "even though I was told a script existed on the line I started " +
            "the search with.",
        };
      }

      // Update the actor to use the new location (reusing a
      // previous breakpoint if it already exists on that line).
      const actualLocation = new GeneratedLocation(generatedSourceActor, actualLine);
      const existingActor = this.breakpointActorMap.getActor(actualLocation);
      this.breakpointActorMap.deleteActor(generatedLocation);
      if (existingActor) {
        actor.delete();
        actor = existingActor;
      } else {
        actor.generatedLocation = actualLocation;
        this.breakpointActorMap.setActor(actualLocation, actor);
      }
    }

    return actor;
  },

  _setBreakpointAtAllGeneratedLocations: function(actor, generatedLocations) {
    let success = false;
    for (const generatedLocation of generatedLocations) {
      if (this._setBreakpointAtGeneratedLocation(
        actor,
        generatedLocation
      )) {
        success = true;
      }
    }
    return success;
  },

  /*
   * Ensure the given BreakpointActor is set as breakpoint handler on all
   * scripts that match the given location in the generated source.
   *
   * @param BreakpointActor actor
   *        The BreakpointActor to be set as a breakpoint handler.
   * @param GeneratedLocation generatedLocation
   *        A GeneratedLocation representing the location in the generated
   *        source for which the given BreakpointActor is to be set as a
   *        breakpoint handler.
   *
   * @returns A Boolean that is true if the BreakpointActor was set as a
   *          breakpoint handler on at least one script, and false otherwise.
   */
  _setBreakpointAtGeneratedLocation: function(actor, generatedLocation) {
    const {
      generatedSourceActor,
      generatedLine,
      generatedColumn,
      generatedLastColumn,
    } = generatedLocation;

    // Find all scripts that match the given source actor and line
    // number.
    let scripts = generatedSourceActor._findDebuggeeScripts(
      { line: generatedLine }
    );

    scripts = scripts.filter((script) => !actor.hasScript(script));

    // Find all entry points that correspond to the given location.
    const entryPoints = [];
    if (generatedColumn === undefined) {
      // This is a line breakpoint, so we are interested in all offsets
      // that correspond to the given line number.
      for (const script of scripts) {
        const offsets = script.getLineOffsets(generatedLine);
        if (offsets.length > 0) {
          entryPoints.push({ script, offsets });
        }
      }
    } else {
      // Compute columnToOffsetMaps for each script so that we can
      // find matching entrypoints for the column breakpoint.
      const columnToOffsetMaps = scripts.map(script =>
        [
          script,
          script.getAllColumnOffsets()
            .filter(({ lineNumber }) => lineNumber === generatedLine),
        ]
      );

      // This is a column breakpoint, so we are interested in all column
      // offsets that correspond to the given line *and* column number.
      for (const [script, columnToOffsetMap] of columnToOffsetMaps) {
        for (const { columnNumber: column, offset } of columnToOffsetMap) {
          if (column >= generatedColumn && column <= generatedLastColumn) {
            entryPoints.push({ script, offsets: [offset] });
          }
        }
      }

      // If we don't find any matching entrypoints,
      // then we should see if the breakpoint comes before or after the column offsets.
      if (entryPoints.length === 0) {
        // It's not entirely clear if the scripts that make it here can come
        // from a variety of sources. This function allows filtering by URL
        // so it seems like it may be possible and we are erring on the side
        // of caution by handling it here.
        const closestScripts = findClosestScriptBySource(
          columnToOffsetMaps.map(pair => pair[0]),
          generatedLine,
          generatedColumn,
        );

        const columnToOffsetLookup = new Map(columnToOffsetMaps);
        for (const script of closestScripts) {
          const columnToOffsetMap = columnToOffsetLookup.get(script);

          if (columnToOffsetMap.length > 0) {
            const firstColumnOffset = columnToOffsetMap[0];
            const lastColumnOffset = columnToOffsetMap[columnToOffsetMap.length - 1];

            if (generatedColumn < firstColumnOffset.columnNumber) {
              entryPoints.push({ script, offsets: [firstColumnOffset.offset] });
            }

            if (generatedColumn > lastColumnOffset.columnNumber) {
              entryPoints.push({ script, offsets: [lastColumnOffset.offset] });
            }
          }
        }
      }
    }

    if (entryPoints.length === 0) {
      return false;
    }

    setBreakpointAtEntryPoints(actor, entryPoints);
    return true;
  },
});

exports.SourceActor = SourceActor;
