/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { assert } = DevToolsUtils;
const EventEmitter = require("devtools/shared/event-emitter");
const { GeneratedLocation } = require("devtools/server/actors/common");

loader.lazyRequireGetter(this, "SourceActor", "devtools/server/actors/source", true);
loader.lazyRequireGetter(this, "isEvalSource", "devtools/server/actors/source", true);

/**
 * Manages the sources for a thread. Handles source maps, locations in the
 * sources, etc for ThreadActors.
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
  // url -> SourceActor
  this._htmlDocumentSourceActors = Object.create(null);
}

/**
 * Matches strings of the form "foo.min.js" or "foo-min.js", etc. If the regular
 * expression matches, we can be fairly sure that the source is minified, and
 * treat it as such.
 */
const MINIFIED_SOURCE_REGEXP = /\bmin\.js$/;

TabSources.prototype = {
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
    this._htmlDocumentSourceActors = Object.create(null);
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
    assert(source,
           "TabSources.prototype.source needs a source");

    if (!this.allowSource(source)) {
      return null;
    }

    // It's a hack, but inline HTML scripts each have real sources,
    // but we want to represent all of them as one source as the
    // HTML page. The actor representing this fake HTML source is
    // stored in this array, which always has a URL, so check it
    // first.
    if (isInlineSource && source.url in this._htmlDocumentSourceActors) {
      return this._htmlDocumentSourceActors[source.url];
    }

    let originalUrl = null;
    if (isInlineSource) {
      // If it's an inline source, the fake HTML source hasn't been
      // created yet (would have returned above), so flip this source
      // into a sourcemapped state by giving it an `originalUrl` which
      // is the HTML url.
      originalUrl = source.url;
      source = null;
    } else if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    const actor = new SourceActor({
      thread: this._thread,
      source: source,
      originalUrl: originalUrl,
      isInlineSource: isInlineSource,
      contentType: contentType,
    });

    this._thread.threadLifetimePool.addActor(actor);

    if (this._autoBlackBox &&
        !this.neverAutoBlackBoxSources.has(actor.url) &&
        this._isMinifiedURL(actor.url)) {
      this.blackBox(actor.url);
      this.neverAutoBlackBoxSources.add(actor.url);
    }

    if (source) {
      this._sourceActors.set(source, actor);
    } else {
      this._htmlDocumentSourceActors[originalUrl] = actor;
    }

    this.emit("newSource", actor);
    return actor;
  },

  _getSourceActor: function(source) {
    if (source.url in this._htmlDocumentSourceActors) {
      return this._htmlDocumentSourceActors[source.url];
    }

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
      throw new Error("getSource: could not find source actor for " +
                      (source.url || "source"));
    }

    return sourceActor;
  },

  getSourceActorByURL: function(url) {
    if (url) {
      for (const [source, actor] of this._sourceActors) {
        if (source.url === url) {
          return actor;
        }
      }

      if (url in this._htmlDocumentSourceActors) {
        return this._htmlDocumentSourceActors[url];
      }
    }
    return null;
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
      return MINIFIED_SOURCE_REGEXP.test(pathname.slice(pathname.lastIndexOf("/") + 1));
    } catch (e) {
      // Not a valid URL so don't try to parse out the filename, just test the
      // whole thing with the minified source regexp.
      return MINIFIED_SOURCE_REGEXP.test(uri);
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

    // Assume the source is inline if the element that introduced it is a
    // script element and does not have a src attribute.
    const element = source.element ? source.element.unsafeDereference() : null;
    if (element && element.tagName === "SCRIPT" && !element.hasAttribute("src")) {
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
      if (url.indexOf("Scratchpad/") === 0 ||
          url.indexOf("javascript:") === 0 ||
          url === "debugger eval code") {
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
    const {lineNumber, columnNumber} = script.getOffsetLocation(offset);
    return new GeneratedLocation(
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
      return new GeneratedLocation();
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
    const index = ranges.findIndex(r => (
      r.end.line <= range.start.line &&
      r.end.column <= range.start.column)
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
    const index = ranges.findIndex(r =>
        r.start.line === range.start.line
      && r.start.column === range.start.column
      && r.end.line === range.end.line
      && r.end.column === range.end.column
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
    const actors = Object.keys(this._htmlDocumentSourceActors).map(k => {
      return this._htmlDocumentSourceActors[k];
    });
    for (const actor of this._sourceActors.values()) {
      actors.push(actor);
    }
    return actors;
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
  return (range.start.line <= line
    || (range.start.line == line && range.start.column <= column))
    && (range.end.line >= line
    || (range.end.line == line && range.end.column >= column));
}

exports.TabSources = TabSources;
exports.isHiddenSource = isHiddenSource;
