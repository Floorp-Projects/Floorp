/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { setBreakpointAtEntryPoints } = require("devtools/server/actors/breakpoint");
const { ActorClassWithSpec } = require("devtools/shared/protocol");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert, fetch } = DevToolsUtils;
const { joinURI } = require("devtools/shared/path");
const { sourceSpec } = require("devtools/shared/specs/source");

loader.lazyRequireGetter(this, "ArrayBufferActor", "devtools/server/actors/array-buffer", true);
loader.lazyRequireGetter(this, "LongStringActor", "devtools/server/actors/string", true);

function isEvalSource(source) {
  const introType = source.introductionType;

  // Script elements that are dynamically created are treated as eval sources.
  // We detect these by looking at whether there was another script on the stack
  // when the source was created.
  if (introType == "scriptElement" && source.introductionScript) {
    return true;
  }

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
 * A SourceActor provides information about the source of a script. Source
 * actors are 1:1 with Debugger.Source objects.
 *
 * @param Debugger.Source source
 *        The source object we are representing.
 * @param ThreadActor thread
 *        The current thread actor.
 * @param Boolean isInlineSource
 *        Optional. True if this is an inline source from a HTML or XUL page.
 * @param String contentType
 *        Optional. The content type of this source, if immediately available.
 */
const SourceActor = ActorClassWithSpec(sourceSpec, {
  typeName: "source",

  initialize: function({ source, thread, isInlineSource, contentType }) {
    this._threadActor = thread;
    this._url = null;
    this._source = source;
    this._contentType = contentType;
    this._isInlineSource = isInlineSource;

    this.onSource = this.onSource.bind(this);
    this._getSourceText = this._getSourceText.bind(this);

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
    if (!this._url) {
      this._url = getSourceURL(this.source, this.threadActor._parent.window);
    }
    return this._url;
  },

  get isCacheEnabled() {
    if (this.threadActor._parent._getCacheDisabled) {
      return !this.threadActor._parent._getCacheDisabled();
    }
    return true;
  },

  form: function() {
    const source = this.source;

    let introductionUrl = null;
    if (source.introductionScript) {
      introductionUrl = source.introductionScript.source.url;
    }

    return {
      actor: this.actorID,
      url: this.url ? this.url.split(" -> ").pop() : null,
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

    query.source = this.source;
    return this.dbg.findScripts(query);
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
    const isWasm = this.source.introductionType === "wasm";

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

  getBreakableLines() {
    const positions = this.getBreakpointPositions();
    const lines = new Set();
    for (const position of positions) {
      if (!lines.has(position.line)) {
        lines.add(position.line);
      }
    }

    return Array.from(lines);
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

    const scripts = this._findDebuggeeScripts();

    const positions = [];
    for (const script of scripts) {
      // This purely a performance boost to avoid needing to build an array
      // of breakable points for scripts when we know we don't need it.
      if (
        script.startLine > endLine ||
        script.startLine + script.lineCount < startLine
      ) {
        continue;
      }

      const offsets = script.getPossibleBreakpoints();
      for (const { lineNumber, columnNumber } of offsets) {
        if (
          lineNumber < startLine ||
          (lineNumber === startLine && columnNumber < startColumn) ||
          lineNumber > endLine ||
          (lineNumber === endLine && columnNumber >= endColumn)
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
      });
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
   * Handler for the "onSource" packet.
   * @return Object
   *         The return of this function contains a field `contentType`, and
   *         a field `source`. `source` can either be an ArrayBuffer or
   *         a LongString.
   */
  onSource: function() {
    return Promise.resolve(this._init)
      .then(this._getSourceText)
      .then(({ content, contentType }) => {
        if (typeof content === "object" && content && content.constructor &&
            content.constructor.name === "ArrayBuffer") {
          return {
            source: new ArrayBufferActor(this.threadActor.conn, content),
            contentType,
          };
        }

        return {
          source: new LongStringActor(this.threadActor.conn, content),
          contentType,
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

  /*
   * Ensure the given BreakpointActor is set as a breakpoint handler on all
   * scripts that match its location in the generated source.
   *
   * @param BreakpointActor actor
   *        The BreakpointActor to be set as a breakpoint handler.
   *
   * @returns A Promise that resolves to the given BreakpointActor.
   */
  applyBreakpoint: function(actor) {
    const { line, column } = actor.location;

    // Find all entry points that correspond to the given location.
    const entryPoints = [];
    if (column === undefined) {
      // Find all scripts that match the given source actor and line
      // number.
      const scripts = this._findDebuggeeScripts({ line })
        .filter((script) => !actor.hasScript(script));

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

      if (lineMatches.length > 0) {
        // A single Debugger.Source may have _multiple_ Debugger.Scripts
        // at the same position from multiple evaluations of the source,
        // so we explicitly want to take all of the matches for the matched
        // column number.
        const firstColumn = lineMatches[0].columnNumber;
        const firstColumnMatches = lineMatches
          .filter(m => m.columnNumber === firstColumn);

        for (const { script, offset } of firstColumnMatches) {
          entryPoints.push({ script, offsets: [offset] });
        }
      }
    } else {
      // Find all scripts that match the given source actor, line,
      // and column number.
      const scripts = this._findDebuggeeScripts({ line, column })
        .filter((script) => !actor.hasScript(script));

      for (const script of scripts) {
        // Check to see if the script contains a breakpoint position at
        // this line and column.
        const possibleBreakpoint = script.getPossibleBreakpoints({
          line,
          minColumn: column,
          maxColumn: column + 1,
        }).pop();

        if (possibleBreakpoint) {
          const { offset } = possibleBreakpoint;
          entryPoints.push({ script, offsets: [offset] });
        }
      }
    }

    setBreakpointAtEntryPoints(actor, entryPoints);
  },
});

exports.SourceActor = SourceActor;
