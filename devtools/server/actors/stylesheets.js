/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const Services = require("Services");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const {fetch} = require("devtools/shared/DevToolsUtils");
const {listenOnce} = require("devtools/shared/async-utils");
const {originalSourceSpec, mediaRuleSpec, styleSheetSpec,
       styleSheetsSpec} = require("devtools/shared/specs/stylesheets");
const {SourceMapConsumer} = require("source-map");
const {
  addPseudoClassLock, removePseudoClassLock } = require("devtools/server/actors/highlighters/utils/markup");

loader.lazyGetter(this, "CssLogic", () => require("devtools/shared/inspector/css-logic"));

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

var TRANSITION_PSEUDO_CLASS = ":-moz-styleeditor-transitioning";
var TRANSITION_DURATION_MS = 500;
var TRANSITION_BUFFER_MS = 1000;
var TRANSITION_RULE_SELECTOR =
`:root${TRANSITION_PSEUDO_CLASS}, :root${TRANSITION_PSEUDO_CLASS} *`;
var TRANSITION_RULE = `${TRANSITION_RULE_SELECTOR} {
  transition-duration: ${TRANSITION_DURATION_MS}ms !important;
  transition-delay: 0ms !important;
  transition-timing-function: ease-out !important;
  transition-property: all !important;
}`;

// The possible kinds of style-applied events.
// UPDATE_PRESERVING_RULES means that the update is guaranteed to
// preserve the number and order of rules on the style sheet.
// UPDATE_GENERAL covers any other kind of change to the style sheet.
const UPDATE_PRESERVING_RULES = 0;
exports.UPDATE_PRESERVING_RULES = UPDATE_PRESERVING_RULES;
const UPDATE_GENERAL = 1;
exports.UPDATE_GENERAL = UPDATE_GENERAL;

// If the user edits a style sheet, we stash a copy of the edited text
// here, keyed by the style sheet.  This way, if the tools are closed
// and then reopened, the edited text will be available.  A weak map
// is used so that navigation by the user will eventually cause the
// edited text to be collected.
let modifiedStyleSheets = new WeakMap();

/**
 * Actor representing an original source of a style sheet that was specified
 * in a source map.
 */
var OriginalSourceActor = protocol.ActorClassWithSpec(originalSourceSpec, {
  initialize: function (url, sourceMap, parentActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.url = url;
    this.sourceMap = sourceMap;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    this.text = null;
  },

  form: function () {
    return {
      actor: this.actorID, // actorID is set when it's added to a pool
      url: this.url,
      relatedStyleSheet: this.parentActor.form()
    };
  },

  _getText: function () {
    if (this.text) {
      return promise.resolve(this.text);
    }
    let content = this.sourceMap.sourceContentFor(this.url);
    if (content) {
      this.text = content;
      return promise.resolve(content);
    }
    let options = {
      policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
      window: this.window
    };
    return fetch(this.url, options).then(({content: text}) => {
      this.text = text;
      return text;
    });
  },

  /**
   * Protocol method to get the text of this source.
   */
  getText: function () {
    return this._getText().then((text) => {
      return new LongStringActor(this.conn, text || "");
    });
  }
});

/**
 * A MediaRuleActor lives on the server and provides access to properties
 * of a DOM @media rule and emits events when it changes.
 */
var MediaRuleActor = protocol.ActorClassWithSpec(mediaRuleSpec, {
  get window() {
    return this.parentActor.window;
  },

  get document() {
    return this.window.document;
  },

  get matches() {
    return this.mql ? this.mql.matches : null;
  },

  initialize: function (mediaRule, parentActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.rawRule = mediaRule;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    this._matchesChange = this._matchesChange.bind(this);

    this.line = DOMUtils.getRuleLine(mediaRule);
    this.column = DOMUtils.getRuleColumn(mediaRule);

    try {
      this.mql = this.window.matchMedia(mediaRule.media.mediaText);
    } catch (e) {
      // Ignored
    }

    if (this.mql) {
      this.mql.addListener(this._matchesChange);
    }
  },

  destroy: function () {
    if (this.mql) {
      this.mql.removeListener(this._matchesChange);
    }

    protocol.Actor.prototype.destroy.call(this);
  },

  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,  // actorID is set when this is added to a pool
      mediaText: this.rawRule.media.mediaText,
      conditionText: this.rawRule.conditionText,
      matches: this.matches,
      line: this.line,
      column: this.column,
      parentStyleSheet: this.parentActor.actorID
    };

    return form;
  },

  _matchesChange: function () {
    this.emit("matches-change", this.matches);
  }
});

/**
 * A StyleSheetActor represents a stylesheet on the server.
 */
var StyleSheetActor = protocol.ActorClassWithSpec(styleSheetSpec, {
  /* List of original sources that generated this stylesheet */
  _originalSources: null,

  toString: function () {
    return "[StyleSheetActor " + this.actorID + "]";
  },

  /**
   * Window of target
   */
  get window() {
    return this._window || this.parentActor.window;
  },

  /**
   * Document of target.
   */
  get document() {
    return this.window.document;
  },

  get ownerNode() {
    return this.rawSheet.ownerNode;
  },

  /**
   * URL of underlying stylesheet.
   */
  get href() {
    return this.rawSheet.href;
  },

  /**
   * Returns the stylesheet href or the document href if the sheet is inline.
   */
  get safeHref() {
    let href = this.href;
    if (!href) {
      if (this.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
        href = this.ownerNode.location.href;
      } else if (this.ownerNode.ownerDocument &&
                 this.ownerNode.ownerDocument.location) {
        href = this.ownerNode.ownerDocument.location.href;
      }
    }
    return href;
  },

  /**
   * Retrieve the index (order) of stylesheet in the document.
   *
   * @return number
   */
  get styleSheetIndex() {
    if (this._styleSheetIndex == -1) {
      for (let i = 0; i < this.document.styleSheets.length; i++) {
        if (this.document.styleSheets[i] == this.rawSheet) {
          this._styleSheetIndex = i;
          break;
        }
      }
    }
    return this._styleSheetIndex;
  },

  destroy: function () {
    if (this._transitionTimeout) {
      this.window.clearTimeout(this._transitionTimeout);
      removePseudoClassLock(
                   this.document.documentElement, TRANSITION_PSEUDO_CLASS);
    }
  },

  initialize: function (styleSheet, parentActor, window) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.rawSheet = styleSheet;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    this._window = window;

    // text and index are unknown until source load
    this.text = null;
    this._styleSheetIndex = -1;
  },

  /**
   * Test whether all the rules in this sheet have associated source.
   * @return {Boolean} true if all the rules have source; false if
   *         some rule was created via CSSOM.
   */
  allRulesHaveSource: function () {
    let rules;
    try {
      rules = this.rawSheet.cssRules;
    } catch (e) {
      // sheet isn't loaded yet
      return true;
    }

    for (let i = 0; i < rules.length; i++) {
      let rule = rules[i];
      if (DOMUtils.getRelativeRuleLine(rule) === 0) {
        return false;
      }
    }

    return true;
  },

  /**
   * Get the raw stylesheet's cssRules once the sheet has been loaded.
   *
   * @return {Promise}
   *         Promise that resolves with a CSSRuleList
   */
  getCSSRules: function () {
    let rules;
    try {
      rules = this.rawSheet.cssRules;
    } catch (e) {
      // sheet isn't loaded yet
    }

    if (rules) {
      return promise.resolve(rules);
    }

    if (!this.ownerNode) {
      return promise.resolve([]);
    }

    if (this._cssRules) {
      return this._cssRules;
    }

    let deferred = defer();

    let onSheetLoaded = (event) => {
      this.ownerNode.removeEventListener("load", onSheetLoaded);

      deferred.resolve(this.rawSheet.cssRules);
    };

    this.ownerNode.addEventListener("load", onSheetLoaded);

    // cache so we don't add many listeners if this is called multiple times.
    this._cssRules = deferred.promise;

    return this._cssRules;
  },

  /**
   * Get the current state of the actor
   *
   * @return {object}
   *         With properties of the underlying stylesheet, plus 'text',
   *        'styleSheetIndex' and 'parentActor' if it's @imported
   */
  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let docHref;
    if (this.ownerNode) {
      if (this.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
        docHref = this.ownerNode.location.href;
      } else if (this.ownerNode.ownerDocument && this.ownerNode.ownerDocument.location) {
        docHref = this.ownerNode.ownerDocument.location.href;
      }
    }

    let form = {
      actor: this.actorID,  // actorID is set when this actor is added to a pool
      href: this.href,
      nodeHref: docHref,
      disabled: this.rawSheet.disabled,
      title: this.rawSheet.title,
      system: !CssLogic.isContentStylesheet(this.rawSheet),
      styleSheetIndex: this.styleSheetIndex
    };

    try {
      form.ruleCount = this.rawSheet.cssRules.length;
    } catch (e) {
      // stylesheet had an @import rule that wasn't loaded yet
      this.getCSSRules().then(() => {
        this._notifyPropertyChanged("ruleCount");
      });
    }
    return form;
  },

  /**
   * Toggle the disabled property of the style sheet
   *
   * @return {object}
   *         'disabled' - the disabled state after toggling.
   */
  toggleDisabled: function () {
    this.rawSheet.disabled = !this.rawSheet.disabled;
    this._notifyPropertyChanged("disabled");

    return this.rawSheet.disabled;
  },

  /**
   * Send an event notifying that a property of the stylesheet
   * has changed.
   *
   * @param  {string} property
   *         Name of the changed property
   */
  _notifyPropertyChanged: function (property) {
    this.emit("property-change", property, this.form()[property]);
  },

  /**
   * Protocol method to get the text of this stylesheet.
   */
  getText: function () {
    return this._getText().then((text) => {
      return new LongStringActor(this.conn, text || "");
    });
  },

  /**
   * Fetch the text for this stylesheet from the cache or network. Return
   * cached text if it's already been fetched.
   *
   * @return {Promise}
   *         Promise that resolves with a string text of the stylesheet.
   */
  _getText: function () {
    if (typeof this.text === "string") {
      return promise.resolve(this.text);
    }

    let cssText = modifiedStyleSheets.get(this.rawSheet);
    if (cssText !== undefined) {
      this.text = cssText;
      return promise.resolve(cssText);
    }

    if (!this.href) {
      // this is an inline <style> sheet
      let content = this.ownerNode.textContent;
      this.text = content;
      return promise.resolve(content);
    }

    return this.fetchStylesheet(this.href).then(({ content }) => {
      this.text = content;
      return content;
    });
  },

  /**
   * Fetch a stylesheet at the provided URL. Returns a promise that will resolve the
   * result of the fetch command.
   *
   * @param  {String} href
   *         The href of the stylesheet to retrieve.
   * @return {Promise} a promise that resolves with an object with the following members
   *         on success:
   *           - content: the document at that URL, as a string,
   *           - contentType: the content type of the document
   *         If an error occurs, the promise is rejected with that error.
   */
  fetchStylesheet: Task.async(function* (href) {
    let options = {
      loadFromCache: true,
      policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
      charset: this._getCSSCharset()
    };

    // Bug 1282660 - We use the system principal to load the default internal
    // stylesheets instead of the content principal since such stylesheets
    // require system principal to load. At meanwhile, we strip the loadGroup
    // for preventing the assertion of the userContextId mismatching.

    // chrome|file|resource|moz-extension protocols rely on the system principal.
    let excludedProtocolsRe = /^(chrome|file|resource|moz-extension):\/\//;
    if (!excludedProtocolsRe.test(this.href)) {
      // Stylesheets using other protocols should use the content principal.
      options.window = this.window;
      options.principal = this.document.nodePrincipal;
    }

    let result;
    try {
      result = yield fetch(this.href, options);
    } catch (e) {
      // The list of excluded protocols can be missing some protocols, try to use the
      // system principal if the first fetch failed.
      console.error(`stylesheets actor: fetch failed for ${this.href},` +
        ` using system principal instead.`);
      options.window = undefined;
      options.principal = undefined;
      result = yield fetch(this.href, options);
    }

    return result;
  }),

  /**
   * Protocol method to get the original source (actors) for this
   * stylesheet if it has uses source maps.
   */
  getOriginalSources: function () {
    if (this._originalSources) {
      return promise.resolve(this._originalSources);
    }
    return this._fetchOriginalSources();
  },

  /**
   * Fetch the original sources (actors) for this style sheet using its
   * source map. If they've already been fetched, returns cached array.
   *
   * @return {Promise}
   *         Promise that resolves with an array of OriginalSourceActors
   */
  _fetchOriginalSources: function () {
    this._clearOriginalSources();
    this._originalSources = [];

    return this.getSourceMap().then((sourceMap) => {
      if (!sourceMap) {
        return null;
      }
      for (let url of sourceMap.sources) {
        let actor = new OriginalSourceActor(url, sourceMap, this);

        this.manage(actor);
        this._originalSources.push(actor);
      }
      return this._originalSources;
    });
  },

  /**
   * Get the SourceMapConsumer for this stylesheet's source map, if
   * it exists. Saves the consumer for later queries.
   *
   * @return {Promise}
   *         A promise that resolves with a SourceMapConsumer, or null.
   */
  getSourceMap: function () {
    if (this._sourceMap) {
      return this._sourceMap;
    }
    return this._fetchSourceMap();
  },

  /**
   * Fetch the source map for this stylesheet.
   *
   * @return {Promise}
   *         A promise that resolves with a SourceMapConsumer, or null.
   */
  _fetchSourceMap: function () {
    let deferred = defer();

    this._getText().then(sheetContent => {
      let url = this._extractSourceMapUrl(sheetContent);
      if (!url) {
        // no source map for this stylesheet
        deferred.resolve(null);
        return;
      }

      url = normalize(url, this.safeHref);
      let options = {
        loadFromCache: false,
        policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
        window: this.window
      };

      let map = fetch(url, options).then(({content}) => {
        // Fetching the source map might have failed with a 404 or other. When
        // this happens, SourceMapConsumer may fail with a JSON.parse error.
        let consumer;
        try {
          consumer = new SourceMapConsumer(content);
        } catch (e) {
          deferred.reject(new Error(
            `Source map at ${url} not found or invalid`));
          return null;
        }
        this._setSourceMapRoot(consumer, url, this.safeHref);
        this._sourceMap = promise.resolve(consumer);

        deferred.resolve(consumer);
        return consumer;
      }, deferred.reject);

      this._sourceMap = map;
    }, deferred.reject);

    return deferred.promise;
  },

  /**
   * Clear and unmanage the original source actors for this stylesheet.
   */
  _clearOriginalSources: function () {
    for (let actor in this._originalSources) {
      this.unmanage(actor);
    }
    this._originalSources = null;
  },

  /**
   * Sets the source map's sourceRoot to be relative to the source map url.
   */
  _setSourceMapRoot: function (sourceMap, absSourceMapURL, scriptURL) {
    if (scriptURL.startsWith("blob:")) {
      scriptURL = scriptURL.replace("blob:", "");
    }
    const base = dirname(
      absSourceMapURL.startsWith("data:")
        ? scriptURL
        : absSourceMapURL);
    sourceMap.sourceRoot = sourceMap.sourceRoot
      ? normalize(sourceMap.sourceRoot, base)
      : base;
  },

  /**
   * Get the source map url specified in the text of a stylesheet.
   *
   * @param  {string} content
   *         The text of the style sheet.
   * @return {string}
   *         Url of source map.
   */
  _extractSourceMapUrl: function (content) {
    // If a SourceMap response header was saved on the style sheet, use it.
    if (this.rawSheet.sourceMapURL) {
      return this.rawSheet.sourceMapURL;
    }
    let matches = /sourceMappingURL\=([^\s\*]*)/.exec(content);
    if (matches) {
      return matches[1];
    }
    return null;
  },

  /**
   * Protocol method that gets the location in the original source of a
   * line, column pair in this stylesheet, if its source mapped, otherwise
   * a promise of the same location.
   */
  getOriginalLocation: function (line, column) {
    return this.getSourceMap().then((sourceMap) => {
      if (sourceMap) {
        return sourceMap.originalPositionFor({ line: line, column: column });
      }
      return {
        fromSourceMap: false,
        source: this.href,
        line: line,
        column: column
      };
    });
  },

  /**
   * Protocol method to get the media rules for the stylesheet.
   */
  getMediaRules: function () {
    return this._getMediaRules();
  },

  /**
   * Get all the @media rules in this stylesheet.
   *
   * @return {promise}
   *         A promise that resolves with an array of MediaRuleActors.
   */
  _getMediaRules: function () {
    return this.getCSSRules().then((rules) => {
      let mediaRules = [];
      for (let i = 0; i < rules.length; i++) {
        let rule = rules[i];
        if (rule.type != Ci.nsIDOMCSSRule.MEDIA_RULE) {
          continue;
        }
        let actor = new MediaRuleActor(rule, this);
        this.manage(actor);

        mediaRules.push(actor);
      }
      return mediaRules;
    });
  },

  /**
   * Get the charset of the stylesheet according to the character set rules
   * defined in <http://www.w3.org/TR/CSS2/syndata.html#charset>.
   * Note that some of the algorithm is implemented in DevToolsUtils.fetch.
   */
  _getCSSCharset: function () {
    let sheet = this.rawSheet;
    if (sheet) {
      // Do we have a @charset rule in the stylesheet?
      // step 2 of syndata.html (without the BOM check).
      if (sheet.cssRules) {
        let rules = sheet.cssRules;
        if (rules.length
            && rules.item(0).type == Ci.nsIDOMCSSRule.CHARSET_RULE) {
          return rules.item(0).encoding;
        }
      }

      // step 3: charset attribute of <link> or <style> element, if it exists
      if (sheet.ownerNode && sheet.ownerNode.getAttribute) {
        let linkCharset = sheet.ownerNode.getAttribute("charset");
        if (linkCharset != null) {
          return linkCharset;
        }
      }

      // step 4 (1 of 2): charset of referring stylesheet.
      let parentSheet = sheet.parentStyleSheet;
      if (parentSheet && parentSheet.cssRules &&
          parentSheet.cssRules[0].type == Ci.nsIDOMCSSRule.CHARSET_RULE) {
        return parentSheet.cssRules[0].encoding;
      }

      // step 4 (2 of 2): charset of referring document.
      if (sheet.ownerNode && sheet.ownerNode.ownerDocument.characterSet) {
        return sheet.ownerNode.ownerDocument.characterSet;
      }
    }

    // step 5: default to utf-8.
    return "UTF-8";
  },

  /**
   * Update the style sheet in place with new text.
   *
   * @param  {object} request
   *         'text' - new text
   *         'transition' - whether to do CSS transition for change.
   *         'kind' - either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   */
  update: function (text, transition, kind = UPDATE_GENERAL) {
    DOMUtils.parseStyleSheet(this.rawSheet, text);

    modifiedStyleSheets.set(this.rawSheet, text);

    this.text = text;

    this._notifyPropertyChanged("ruleCount");

    if (transition) {
      this._insertTransistionRule(kind);
    } else {
      this.emit("style-applied", kind, this);
    }

    this._getMediaRules().then((rules) => {
      this.emit("media-rules-changed", rules);
    });
  },

  /**
   * Insert a catch-all transition rule into the document. Set a timeout
   * to remove the rule after a certain time.
   */
  _insertTransistionRule: function (kind) {
    addPseudoClassLock(this.document.documentElement, TRANSITION_PSEUDO_CLASS);

    // We always add the rule since we've just reset all the rules
    this.rawSheet.insertRule(TRANSITION_RULE, this.rawSheet.cssRules.length);

    // Set up clean up and commit after transition duration (+buffer)
    // @see _onTransitionEnd
    this.window.clearTimeout(this._transitionTimeout);
    this._transitionTimeout = this.window.setTimeout(
      this._onTransitionEnd.bind(this, kind),
      TRANSITION_DURATION_MS + TRANSITION_BUFFER_MS);
  },

  /**
   * This cleans up class and rule added for transition effect and then
   * notifies that the style has been applied.
   */
  _onTransitionEnd: function (kind) {
    this._transitionTimeout = null;
    removePseudoClassLock(this.document.documentElement, TRANSITION_PSEUDO_CLASS);

    let index = this.rawSheet.cssRules.length - 1;
    let rule = this.rawSheet.cssRules[index];
    if (rule.selectorText == TRANSITION_RULE_SELECTOR) {
      this.rawSheet.deleteRule(index);
    }

    this.emit("style-applied", kind, this);
  }
});

exports.StyleSheetActor = StyleSheetActor;

/**
 * Creates a StyleSheetsActor. StyleSheetsActor provides remote access to the
 * stylesheets of a document.
 */
var StyleSheetsActor = protocol.ActorClassWithSpec(styleSheetsSpec, {
  /**
   * The window we work with, taken from the parent actor.
   */
  get window() {
    return this.parentActor.window;
  },

  /**
   * The current content document of the window we work with.
   */
  get document() {
    return this.window.document;
  },

  form: function () {
    return { actor: this.actorID };
  },

  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.parentActor = tabActor;
  },

  /**
   * Protocol method for getting a list of StyleSheetActors representing
   * all the style sheets in this document.
   */
  getStyleSheets: Task.async(function* () {
    // Iframe document can change during load (bug 1171919). Track their windows
    // instead.
    let windows = [this.window];
    let actors = [];

    for (let win of windows) {
      let sheets = yield this._addStyleSheets(win);
      actors = actors.concat(sheets);

      // Recursively handle style sheets of the documents in iframes.
      for (let iframe of win.document.querySelectorAll("iframe, browser, frame")) {
        if (iframe.contentDocument && iframe.contentWindow) {
          // Sometimes, iframes don't have any document, like the
          // one that are over deeply nested (bug 285395)
          windows.push(iframe.contentWindow);
        }
      }
    }
    return actors;
  }),

  /**
   * Check if we should be showing this stylesheet.
   *
   * @param {Document} doc
   *        Document for which we're checking
   * @param {DOMCSSStyleSheet} sheet
   *        Stylesheet we're interested in
   *
   * @return boolean
   *         Whether the stylesheet should be listed.
   */
  _shouldListSheet: function (doc, sheet) {
    // Special case about:PreferenceStyleSheet, as it is generated on the
    // fly and the URI is not registered with the about: handler.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
    if (sheet.href && sheet.href.toLowerCase() == "about:preferencestylesheet") {
      return false;
    }

    return true;
  },

  /**
   * Add all the stylesheets for the document in this window to the map and
   * create an actor for each one if not already created.
   *
   * @param {Window} win
   *        Window for which to add stylesheets
   *
   * @return {Promise}
   *         Promise that resolves to an array of StyleSheetActors
   */
  _addStyleSheets: function (win) {
    return Task.spawn(function* () {
      let doc = win.document;
      // readyState can be uninitialized if an iframe has just been created but
      // it has not started to load yet.
      if (doc.readyState === "loading" || doc.readyState === "uninitialized") {
        // Wait for the document to load first.
        yield listenOnce(win, "DOMContentLoaded", true);

        // Make sure we have the actual document for this window. If the
        // readyState was initially uninitialized, the initial dummy document
        // was replaced with the actual document (bug 1171919).
        doc = win.document;
      }

      let isChrome = Services.scriptSecurityManager.isSystemPrincipal(doc.nodePrincipal);
      let styleSheets = isChrome ? DOMUtils.getAllStyleSheets(doc) : doc.styleSheets;
      let actors = [];
      for (let i = 0; i < styleSheets.length; i++) {
        let sheet = styleSheets[i];
        if (!this._shouldListSheet(doc, sheet)) {
          continue;
        }

        let actor = this.parentActor.createStyleSheetActor(sheet);
        actors.push(actor);

        // Get all sheets, including imported ones
        let imports = yield this._getImported(doc, actor);
        actors = actors.concat(imports);
      }
      return actors;
    }.bind(this));
  },

  /**
   * Get all the stylesheets @imported from a stylesheet.
   *
   * @param  {Document} doc
   *         The document including the stylesheet
   * @param  {DOMStyleSheet} styleSheet
   *         Style sheet to search
   * @return {Promise}
   *         A promise that resolves with an array of StyleSheetActors
   */
  _getImported: function (doc, styleSheet) {
    return Task.spawn(function* () {
      let rules = yield styleSheet.getCSSRules();
      let imported = [];

      for (let i = 0; i < rules.length; i++) {
        let rule = rules[i];
        if (rule.type == Ci.nsIDOMCSSRule.IMPORT_RULE) {
          // With the Gecko style system, the associated styleSheet may be null
          // if it has already been seen because an import cycle for the same
          // URL.  With Stylo, the styleSheet will exist (which is correct per
          // the latest CSSOM spec), so we also need to check ancestors for the
          // same URL to avoid cycles.
          let sheet = rule.styleSheet;
          if (!sheet || this._haveAncestorWithSameURL(sheet) ||
              !this._shouldListSheet(doc, sheet)) {
            continue;
          }
          let actor = this.parentActor.createStyleSheetActor(rule.styleSheet);
          imported.push(actor);

          // recurse imports in this stylesheet as well
          let children = yield this._getImported(doc, actor);
          imported = imported.concat(children);
        } else if (rule.type != Ci.nsIDOMCSSRule.CHARSET_RULE) {
          // @import rules must precede all others except @charset
          break;
        }
      }

      return imported;
    }.bind(this));
  },

  /**
   * Check all ancestors to see if this sheet's URL matches theirs as a way to
   * detect an import cycle.
   *
   * @param {DOMStyleSheet} sheet
   */
  _haveAncestorWithSameURL(sheet) {
    let sheetHref = sheet.href;
    while (sheet.parentStyleSheet) {
      if (sheet.parentStyleSheet.href == sheetHref) {
        return true;
      }
      sheet = sheet.parentStyleSheet;
    }
    return false;
  },

  /**
   * Create a new style sheet in the document with the given text.
   * Return an actor for it.
   *
   * @param  {object} request
   *         Debugging protocol request object, with 'text property'
   * @return {object}
   *         Object with 'styelSheet' property for form on new actor.
   */
  addStyleSheet: function (text) {
    let parent = this.document.documentElement;
    let style = this.document.createElementNS("http://www.w3.org/1999/xhtml", "style");
    style.setAttribute("type", "text/css");

    if (text) {
      style.appendChild(this.document.createTextNode(text));
    }
    parent.appendChild(style);

    let actor = this.parentActor.createStyleSheetActor(style.sheet);
    return actor;
  }
});

exports.StyleSheetsActor = StyleSheetsActor;

/**
 * Normalize multiple relative paths towards the base paths on the right.
 */
function normalize(...urls) {
  let base = Services.io.newURI(urls.pop());
  let url;
  while ((url = urls.pop())) {
    base = Services.io.newURI(url, null, base);
  }
  return base.spec;
}

function dirname(path) {
  return Services.io.newURI(
    ".", null, Services.io.newURI(path)).spec;
}
