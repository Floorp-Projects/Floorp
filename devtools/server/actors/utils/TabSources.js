/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert, fetch } = DevToolsUtils;
const EventEmitter = require("devtools/shared/event-emitter");
const { SourceLocation } = require("devtools/server/actors/common");
const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "SourceActor",
  "devtools/server/actors/source",
  true
);
loader.lazyRequireGetter(
  this,
  "isEvalSource",
  "devtools/server/actors/source",
  true
);

/**
 * Manages the sources for a thread. Handles URL contents, locations in
 * the sources, etc for ThreadActors.
 */
function TabSources(threadActor, allowSourceFn = () => true) {
  EventEmitter.decorate(this);

  this._thread = threadActor;
  this._autoBlackBox = true;
  this.allowSource = source => {
    return !isHiddenSource(source) && allowSourceFn(source);
  };

  this.blackBoxedSources = new Map();
  this.neverAutoBlackBoxSources = new Set();

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

/**
 * Matches strings of the form "foo.min.js" or "foo-min.js", etc. If the regular
 * expression matches, we can be fairly sure that the source is minified, and
 * treat it as such.
 */
const MINIFIED_SOURCE_REGEXP = /\bmin\.js$/;

TabSources.prototype = {
  destroy() {
    if (!isWorker) {
      Services.obs.removeObserver(this, "devtools-html-content");
    }
  },

  /**
   * Update preferences and clear out existing sources
   */
  setOptions: function(options) {
    let shouldReset = false;

    if ("autoBlackBox" in options) {
      shouldReset = true;
      this._autoBlackBox = options.autoBlackBox;
    }

    if (shouldReset) {
      this.reset();
    }
  },

  /**
   * Clear existing sources so they are recreated on the next access.
   */
  reset: function() {
    this._sourceActors = new Map();
    this._urlContents = new Map();
    this._urlWaiters = new Map();
    this._sourcesByInternalSourceId = null;
  },

  /**
   * Return the source actor representing the `source` (or
   * `originalUrl`), creating one if none exists already. May return
   * null if the source is disallowed.
   *
   * @param Debugger.Source source
   *        The source to make an actor for
   * @param boolean isInlineSource
   *        True if this source is an inline HTML source, and should thus be
   *        treated as a subsection of a larger source file.
   * @param optional String contentType
   *        The content type of the source, if immediately available.
   * @returns a SourceActor representing the source or null.
   */
  source: function({ source, isInlineSource, contentType }) {
    assert(source, "TabSources.prototype.source needs a source");

    if (!this.allowSource(source)) {
      return null;
    }

    if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    const actor = new SourceActor({
      thread: this._thread,
      source,
      isInlineSource,
      contentType,
    });

    this._thread.threadLifetimePool.addActor(actor);

    if (
      this._autoBlackBox &&
      !this.neverAutoBlackBoxSources.has(actor.url) &&
      this._isMinifiedURL(actor.url)
    ) {
      this.blackBox(actor.url);
      this.neverAutoBlackBoxSources.add(actor.url);
    }

    this._sourceActors.set(source, actor);
    if (this._sourcesByInternalSourceId && source.id) {
      this._sourcesByInternalSourceId.set(source.id, source);
    }

    this.emit("newSource", actor);
    return actor;
  },

  _getSourceActor: function(source) {
    if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    return null;
  },

  hasSourceActor: function(source) {
    return !!this._getSourceActor(source);
  },

  getSourceActor: function(source) {
    const sourceActor = this._getSourceActor(source);

    if (!sourceActor) {
      throw new Error(
        "getSource: could not find source actor for " + (source.url || "source")
      );
    }

    return sourceActor;
  },

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
  },

  getSourceActorByInternalSourceId: function(id) {
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
  },

  getSourceActorsByURL: function(url) {
    const rv = [];
    if (url) {
      for (const [, actor] of this._sourceActors) {
        if (actor.url === url) {
          rv.push(actor);
        }
      }
    }
    return rv;
  },

  getSourceActorById(actorId) {
    for (const [, actor] of this._sourceActors) {
      if (actor.actorID == actorId) {
        return actor;
      }
    }
    return null;
  },

  /**
   * Returns true if the URL likely points to a minified resource, false
   * otherwise.
   *
   * @param String uri
   *        The url to test.
   * @returns Boolean
   */
  _isMinifiedURL: function(uri) {
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
  },

  /**
   * Return whether a source represents an inline script.
   */
  isInlineScript(source) {
    // Assume the source is inline if the element that introduced it is a
    // script element and does not have a src attribute.
    try {
      const e = source.element ? source.element.unsafeDereference() : null;
      return e && e.tagName === "SCRIPT" && !e.hasAttribute("src");
    } catch (e) {
      // If we are debugging a dead window then the above can throw.
      DevToolsUtils.reportException("TabSources.isInlineScript", e);
      return false;
    }
  },

  /**
   * Create a source actor representing this source.
   *
   * @param Debugger.Source source
   *        The source instance to create an actor for.
   * @returns SourceActor
   */
  createSourceActor: function(source) {
    // Don't use getSourceURL because we don't want to consider the
    // displayURL property if it's an eval source. We only want to
    // consider real URLs, otherwise if there is a URL but it's
    // invalid the code below will not set the content type, and we
    // will later try to fetch the contents of the URL to figure out
    // the content type, but it's a made up URL for eval sources.
    const url = isEvalSource(source) ? null : source.url;
    const spec = { source };

    // XXX bug 915433: We can't rely on Debugger.Source.prototype.text
    // if the source is an HTML-embedded <script> tag. Since we don't
    // have an API implemented to detect whether this is the case, we
    // need to be conservative and only treat valid js files as real
    // sources. Otherwise, use the `originalUrl` property to treat it
    // as an HTML source that manages multiple inline sources.

    if (this.isInlineScript(source)) {
      if (source.introductionScript) {
        // As for other evaluated sources, script elements which were
        // dynamically generated when another script ran should have
        // a javascript content-type.
        spec.contentType = "text/javascript";
      } else {
        spec.isInlineSource = true;
      }
    } else if (source.introductionType === "wasm") {
      // Wasm sources are not JavaScript. Give them their own content-type.
      spec.contentType = "text/wasm";
    } else if (source.introductionType === "debugger eval") {
      // All debugger eval code should have a text/javascript content-type.
      // See Bug 1399064
      spec.contentType = "text/javascript";
    } else if (url) {
      // There are a few special URLs that we know are JavaScript:
      // inline `javascript:` and code coming from the console
      if (
        url.indexOf("Scratchpad/") === 0 ||
        url.indexOf("javascript:") === 0 ||
        url === "debugger eval code"
      ) {
        spec.contentType = "text/javascript";
      } else {
        try {
          const pathname = new URL(url).pathname;
          const filename = pathname.slice(pathname.lastIndexOf("/") + 1);
          const index = filename.lastIndexOf(".");
          const extension = index >= 0 ? filename.slice(index + 1) : "";
          if (extension === "xml") {
            // XUL inline scripts may not correctly have the
            // `source.element` property, so do a blunt check here if
            // it's an xml page.
            spec.isInlineSource = true;
          } else if (extension === "js") {
            spec.contentType = "text/javascript";
          }
        } catch (e) {
          // This only needs to be here because URL is not yet exposed to
          // workers. (BUG 1258892)
          const filename = url;
          const index = filename.lastIndexOf(".");
          const extension = index >= 0 ? filename.slice(index + 1) : "";
          if (extension === "js") {
            spec.contentType = "text/javascript";
          }
        }
      }
    } else {
      // Assume the content is javascript if there's no URL
      spec.contentType = "text/javascript";
    }

    return this.source(spec);
  },

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
  getScriptOffsetLocation: function(script, offset) {
    const { lineNumber, columnNumber } = script.getOffsetMetadata(offset);
    return new SourceLocation(
      this.createSourceActor(script.source),
      lineNumber,
      columnNumber
    );
  },

  /**
   * Return the non-source-mapped location of the given Debugger.Frame. If the
   * frame does not have a script, the location's properties are all null.
   *
   * @param Debugger.Frame frame
   *        The frame whose location we are getting.
   * @returns Object
   *          Returns an object of the form { source, line, column }
   */
  getFrameLocation: function(frame) {
    if (!frame || !frame.script) {
      return new SourceLocation();
    }
    return this.getScriptOffsetLocation(frame.script, frame.offset);
  },

  /**
   * Returns true if URL for the given source is black boxed.
   *
   *   * @param url String
   *        The URL of the source which we are checking whether it is black
   *        boxed or not.
   */
  isBlackBoxed: function(url, line, column) {
    const ranges = this.blackBoxedSources.get(url);
    if (!ranges) {
      return this.blackBoxedSources.has(url);
    }

    const range = ranges.find(r => isLocationInRange({ line, column }, r));
    return !!range;
  },

  isFrameBlackBoxed: function(frame) {
    const location = this.getFrameLocation(frame);
    return this.isBlackBoxed(location.url);
  },

  /**
   * Add the given source URL to the set of sources that are black boxed.
   *
   * @param url String
   *        The URL of the source which we are black boxing.
   */
  blackBox: function(url, range) {
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
  },

  /**
   * Remove the given source URL to the set of sources that are black boxed.
   *
   * @param url String
   *        The URL of the source which we are no longer black boxing.
   */
  unblackBox: function(url, range) {
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
  },

  iter: function() {
    return [...this._sourceActors.values()];
  },

  /**
   * Listener for new HTML content.
   */
  observe(subject, topic, data) {
    if (topic == "devtools-html-content") {
      const { parserID, uri, contents, complete } = JSON.parse(data);
      if (this._urlContents.has(uri)) {
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
      } else {
        this._urlContents.set(uri, {
          content: contents,
          complete,
          contentType: "text/html",
          parserID,
        });
      }
    }
  },

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

    return this._fetchURLContents(url, partial, canUseCache);
  },

  _fetchURLContents: async function(url, partial, canUseCache) {
    // Only try the cache if it is currently enabled for the document.
    // Without this check, the cache may return stale data that doesn't match
    // the document shown in the browser.
    let loadFromCache = canUseCache;
    if (canUseCache && this._thread._parent._getCacheDisabled) {
      loadFromCache = !this._thread._parent._getCacheDisabled();
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

    this._urlContents.set(url, { ...result, complete: true });

    return result;
  },

  _reportLoadSourceError: function(error) {
    try {
      DevToolsUtils.reportException("SourceActor", error);

      const lines = JSON.stringify(this.form(), null, 4).split(/\n/g);
      lines.forEach(line => console.error("\t", line));
    } catch (e) {
      // ignore
    }
  },
};

/*
 * Checks if a source should never be displayed to the user because
 * it's either internal or we don't support in the UI yet.
 */
function isHiddenSource(source) {
  return source.introductionType === "Function.prototype";
}

function isLocationInRange({ line, column }, range) {
  return (
    (range.start.line <= line ||
      (range.start.line == line && range.start.column <= column)) &&
    (range.end.line >= line ||
      (range.end.line == line && range.end.column >= column))
  );
}

exports.TabSources = TabSources;
exports.isHiddenSource = isHiddenSource;
