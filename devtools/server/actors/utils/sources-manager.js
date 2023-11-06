/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
const { assert, fetch } = DevToolsUtils;
const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  SourceLocation,
} = require("resource://devtools/server/actors/common.js");

loader.lazyRequireGetter(
  this,
  "SourceActor",
  "resource://devtools/server/actors/source.js",
  true
);

/**
 * Matches strings of the form "foo.min.js" or "foo-min.js", etc. If the regular
 * expression matches, we can be fairly sure that the source is minified, and
 * treat it as such.
 */
const MINIFIED_SOURCE_REGEXP = /\bmin\.js$/;

/**
 * Manages the sources for a thread. Handles URL contents, locations in
 * the sources, etc for ThreadActors.
 */
class SourcesManager extends EventEmitter {
  constructor(threadActor) {
    super();
    this._thread = threadActor;

    this.blackBoxedSources = new Map();

    // Debugger.Source -> SourceActor
    this._sourceActors = new Map();

    // URL -> content
    //
    // Any possibly incomplete content that has been loaded for each URL.
    this._urlContents = new Map();

    // URL -> Promise[]
    //
    // Any promises waiting on a URL to be completely loaded.
    this._urlWaiters = new Map();

    // Debugger.Source.id -> Debugger.Source
    //
    // The IDs associated with ScriptSources and available via DebuggerSource.id
    // are internal to this process and should not be exposed to the client. This
    // map associates these IDs with the corresponding source, provided the source
    // has not been GC'ed and the actor has been created. This is lazily populated
    // the first time it is needed.
    this._sourcesByInternalSourceId = null;

    if (!isWorker) {
      Services.obs.addObserver(this, "devtools-html-content");
    }
  }

  destroy() {
    if (!isWorker) {
      Services.obs.removeObserver(this, "devtools-html-content");
    }
  }

  /**
   * Clear existing sources so they are recreated on the next access.
   */
  reset() {
    this._sourceActors = new Map();
    this._urlContents = new Map();
    this._urlWaiters = new Map();
    this._sourcesByInternalSourceId = null;
  }

  /**
   * Create a source actor representing this source.
   *
   * @param Debugger.Source source
   *        The source to make an actor for.
   * @returns a SourceActor representing the source.
   */
  createSourceActor(source) {
    assert(source, "SourcesManager.prototype.source needs a source");

    if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    const actor = new SourceActor({
      thread: this._thread,
      source,
    });

    this._thread.threadLifetimePool.manage(actor);

    this._sourceActors.set(source, actor);
    if (this._sourcesByInternalSourceId && source.id) {
      this._sourcesByInternalSourceId.set(source.id, source);
    }

    this.emit("newSource", actor);
    return actor;
  }

  _getSourceActor(source) {
    if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    return null;
  }

  hasSourceActor(source) {
    return !!this._getSourceActor(source);
  }

  getSourceActor(source) {
    const sourceActor = this._getSourceActor(source);

    if (!sourceActor) {
      throw new Error(
        "getSource: could not find source actor for " + (source.url || "source")
      );
    }

    return sourceActor;
  }

  getOrCreateSourceActor(source) {
    // Tolerate the source coming from a different Debugger than the one
    // associated with the thread.
    try {
      source = this._thread.dbg.adoptSource(source);
    } catch (e) {
      // We can't create actors for sources in the same compartment as the
      // thread's Debugger.
      if (/is in the same compartment as this debugger/.test(e)) {
        return null;
      }
      throw e;
    }

    if (this.hasSourceActor(source)) {
      return this.getSourceActor(source);
    }
    return this.createSourceActor(source);
  }

  getSourceActorByInternalSourceId(id) {
    if (!this._sourcesByInternalSourceId) {
      this._sourcesByInternalSourceId = new Map();
      for (const source of this._thread.dbg.findSources()) {
        if (source.id) {
          this._sourcesByInternalSourceId.set(source.id, source);
        }
      }
    }
    const source = this._sourcesByInternalSourceId.get(id);
    if (source) {
      return this.getOrCreateSourceActor(source);
    }
    return null;
  }

  getSourceActorsByURL(url) {
    const rv = [];
    if (url) {
      for (const [, actor] of this._sourceActors) {
        if (actor.url === url) {
          rv.push(actor);
        }
      }
    }
    return rv;
  }

  getSourceActorById(actorId) {
    for (const [, actor] of this._sourceActors) {
      if (actor.actorID == actorId) {
        return actor;
      }
    }
    return null;
  }

  /**
   * Returns true if the URL likely points to a minified resource, false
   * otherwise.
   *
   * @param String uri
   *        The url to test.
   * @returns Boolean
   */
  _isMinifiedURL(uri) {
    if (!uri) {
      return false;
    }

    try {
      const url = new URL(uri);
      const pathname = url.pathname;
      return MINIFIED_SOURCE_REGEXP.test(
        pathname.slice(pathname.lastIndexOf("/") + 1)
      );
    } catch (e) {
      // Not a valid URL so don't try to parse out the filename, just test the
      // whole thing with the minified source regexp.
      return MINIFIED_SOURCE_REGEXP.test(uri);
    }
  }

  /**
   * Return the non-source-mapped location of an offset in a script.
   *
   * @param Debugger.Script script
   *        The script associated with the offset.
   * @param Number offset
   *        Offset within the script of the location.
   * @returns Object
   *          Returns an object of the form { source, line, column }
   */
  getScriptOffsetLocation(script, offset) {
    const { lineNumber, columnNumber } = script.getOffsetMetadata(offset);
    return new SourceLocation(
      this.createSourceActor(script.source),
      lineNumber,
      columnNumber
    );
  }

  /**
   * Return the non-source-mapped location of the given Debugger.Frame. If the
   * frame does not have a script, the location's properties are all null.
   *
   * @param Debugger.Frame frame
   *        The frame whose location we are getting.
   * @returns Object
   *          Returns an object of the form { source, line, column }
   */
  getFrameLocation(frame) {
    if (!frame || !frame.script) {
      return new SourceLocation();
    }
    return this.getScriptOffsetLocation(frame.script, frame.offset);
  }

  /**
   * Returns true if URL for the given source is black boxed.
   *
   *   * @param url String
   *        The URL of the source which we are checking whether it is black
   *        boxed or not.
   */
  isBlackBoxed(url, line, column) {
    if (!this.blackBoxedSources.has(url)) {
      return false;
    }

    const ranges = this.blackBoxedSources.get(url);

    // If we have an entry in the map, but it is falsy, the source is fully blackboxed.
    if (!ranges) {
      return true;
    }

    const range = ranges.find(r => isLocationInRange({ line, column }, r));
    return !!range;
  }

  isFrameBlackBoxed(frame) {
    const { url, line, column } = this.getFrameLocation(frame);
    return this.isBlackBoxed(url, line, column);
  }

  clearAllBlackBoxing() {
    this.blackBoxedSources.clear();
  }

  /**
   * Add the given source URL to the set of sources that are black boxed.
   *
   * @param url String
   *        The URL of the source which we are black boxing.
   */
  blackBox(url, range) {
    if (!range) {
      // blackbox the whole source
      return this.blackBoxedSources.set(url, null);
    }

    const ranges = this.blackBoxedSources.get(url) || [];
    // ranges are sorted in ascening order
    const index = ranges.findIndex(
      r => r.end.line <= range.start.line && r.end.column <= range.start.column
    );

    ranges.splice(index + 1, 0, range);
    this.blackBoxedSources.set(url, ranges);
    return true;
  }

  /**
   * Remove the given source URL to the set of sources that are black boxed.
   *
   * @param url String
   *        The URL of the source which we are no longer black boxing.
   */
  unblackBox(url, range) {
    if (!range) {
      return this.blackBoxedSources.delete(url);
    }

    const ranges = this.blackBoxedSources.get(url);
    const index = ranges.findIndex(
      r =>
        r.start.line === range.start.line &&
        r.start.column === range.start.column &&
        r.end.line === range.end.line &&
        r.end.column === range.end.column
    );

    if (index !== -1) {
      ranges.splice(index, 1);
    }

    if (ranges.length === 0) {
      return this.blackBoxedSources.delete(url);
    }

    return this.blackBoxedSources.set(url, ranges);
  }

  iter() {
    return [...this._sourceActors.values()];
  }

  /**
   * Listener for new HTML content.
   */
  observe(subject, topic, data) {
    if (topic == "devtools-html-content") {
      const { parserID, uri, contents, complete } = JSON.parse(data);
      if (this._urlContents.has(uri)) {
        // We received many devtools-html-content events, if we already received one,
        // aggregate the data with the one we already received.
        const existing = this._urlContents.get(uri);
        if (existing.parserID == parserID) {
          assert(!existing.complete);
          existing.content = existing.content + contents;
          existing.complete = complete;

          // After the HTML has finished loading, resolve any promises
          // waiting for the complete file contents. Waits will only
          // occur when the URL was ever partially loaded.
          if (complete) {
            const waiters = this._urlWaiters.get(uri);
            if (waiters) {
              for (const waiter of waiters) {
                waiter();
              }
              this._urlWaiters.delete(uri);
            }
          }
        }
      } else if (contents) {
        // Ensure that `contents` is non-empty. We may miss all the devtools-html-content events except the last
        // one which has a empty `contents` and complete set to true.
        // This reproduces when opening a same-process iframe. In this particular scenario, we instantiate the target and thread actor
        // on `DOMDocElementInserted` and the HTML document is already parsed, but we still receive this one very last notification.
        this._urlContents.set(uri, {
          content: contents,
          complete,
          contentType: "text/html",
          parserID,
        });
      }
    }
  }

  /**
   * Get the contents of a URL, fetching it if necessary. If partial is set and
   * any content for the URL has been received, that partial content is returned
   * synchronously.
   */
  urlContents(url, partial, canUseCache) {
    if (this._urlContents.has(url)) {
      const data = this._urlContents.get(url);
      if (!partial && !data.complete) {
        return new Promise(resolve => {
          if (!this._urlWaiters.has(url)) {
            this._urlWaiters.set(url, []);
          }
          this._urlWaiters.get(url).push(resolve);
        }).then(() => {
          assert(data.complete);
          return {
            content: data.content,
            contentType: data.contentType,
          };
        });
      }
      return {
        content: data.content,
        contentType: data.contentType,
      };
    }
    if (partial) {
      return {
        content: "",
        contentType: "",
      };
    }
    return this._fetchURLContents(url, partial, canUseCache);
  }

  async _fetchURLContents(url, partial, canUseCache) {
    // Only try the cache if it is currently enabled for the document.
    // Without this check, the cache may return stale data that doesn't match
    // the document shown in the browser.
    let loadFromCache = canUseCache;
    if (canUseCache && this._thread._parent.browsingContext) {
      loadFromCache = !(
        this._thread._parent.browsingContext.defaultLoadFlags ===
        Ci.nsIRequest.LOAD_BYPASS_CACHE
      );
    }

    // Fetch the sources with the same principal as the original document
    const win = this._thread._parent.window;
    let principal, cacheKey;
    // On xpcshell, we don't have a window but a Sandbox
    if (!isWorker && win instanceof Ci.nsIDOMWindow) {
      const docShell = win.docShell;
      const channel = docShell.currentDocumentChannel;
      principal = channel.loadInfo.loadingPrincipal;

      // Retrieve the cacheKey in order to load POST requests from cache
      // Note that chrome:// URLs don't support this interface.
      if (
        loadFromCache &&
        docShell.currentDocumentChannel instanceof Ci.nsICacheInfoChannel
      ) {
        cacheKey = docShell.currentDocumentChannel.cacheKey;
      }
    }

    let result;
    try {
      result = await fetch(url, {
        principal,
        cacheKey,
        loadFromCache,
      });
    } catch (error) {
      this._reportLoadSourceError(error);
      throw error;
    }

    // When we fetch the contents, there is a risk that the contents we get
    // do not match up with the actual text of the sources these contents will
    // be associated with. We want to always show contents that include that
    // actual text (otherwise it will be very confusing or unusable for users),
    // so replace the contents with the actual text if there is a mismatch.
    const actors = [...this._sourceActors.values()].filter(
      actor => actor.url == url
    );
    if (!actors.every(actor => actor.contentMatches(result))) {
      if (actors.length > 1) {
        // When there are multiple actors we won't be able to show the source
        // for all of them. Ask the user to reload so that we don't have to do
        // any fetching.
        result.content = "Error: Incorrect contents fetched, please reload.";
      } else {
        result.content = actors[0].actualText();
      }
    }

    this._urlContents.set(url, { ...result, complete: true });

    return result;
  }

  _reportLoadSourceError(error) {
    try {
      DevToolsUtils.reportException("SourceActor", error);

      const lines = JSON.stringify(this.form(), null, 4).split(/\n/g);
      lines.forEach(line => console.error("\t", line));
    } catch (e) {
      // ignore
    }
  }
}

function isLocationInRange({ line, column }, range) {
  return (
    (range.start.line <= line ||
      (range.start.line == line && range.start.column <= column)) &&
    (range.end.line >= line ||
      (range.end.line == line && range.end.column >= column))
  );
}

exports.SourcesManager = SourcesManager;
