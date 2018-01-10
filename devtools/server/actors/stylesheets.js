/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci} = require("chrome");
const Services = require("Services");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const {Task} = require("devtools/shared/task");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const {fetch} = require("devtools/shared/DevToolsUtils");
const {mediaRuleSpec, styleSheetSpec,
       styleSheetsSpec} = require("devtools/shared/specs/stylesheets");
const {
  addPseudoClassLock, removePseudoClassLock } = require("devtools/server/actors/highlighters/utils/markup");

loader.lazyRequireGetter(this, "CssLogic", "devtools/shared/inspector/css-logic");
loader.lazyRequireGetter(this, "addPseudoClassLock",
  "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "removePseudoClassLock",
  "devtools/server/actors/highlighters/utils/markup", true);
loader.lazyRequireGetter(this, "loadSheet", "devtools/shared/layout/utils", true);

loader.lazyServiceGetter(this, "DOMUtils", "@mozilla.org/inspector/dom-utils;1", "inIDOMUtils");

var TRANSITION_PSEUDO_CLASS = ":-moz-styleeditor-transitioning";
var TRANSITION_DURATION_MS = 500;
var TRANSITION_BUFFER_MS = 1000;
var TRANSITION_RULE_SELECTOR =
`:root${TRANSITION_PSEUDO_CLASS}, :root${TRANSITION_PSEUDO_CLASS} *`;

var TRANSITION_SHEET = "data:text/css;charset=utf-8," + encodeURIComponent(`
  ${TRANSITION_RULE_SELECTOR} {
    transition-duration: ${TRANSITION_DURATION_MS}ms !important;
    transition-delay: 0ms !important;
    transition-timing-function: ease-out !important;
    transition-property: all !important;
  }
`);

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
  toString: function () {
    return "[StyleSheetActor " + this.actorID + "]";
  },

  /**
   * Window of target
   */
  get window() {
    return this.parentActor.window;
  },

  /**
   * Document of target.
   */
  get document() {
    return this.window.document;
  },

  /**
   * StyleSheet's window.
   */
  get ownerWindow() {
    // eslint-disable-next-line mozilla/use-ownerGlobal
    return this.ownerDocument.defaultView;
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
    if (this._transitionTimeout && this.window) {
      this.window.clearTimeout(this._transitionTimeout);
      removePseudoClassLock(
                   this.document.documentElement, TRANSITION_PSEUDO_CLASS);
    }
  },

  initialize: function (styleSheet, parentActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.rawSheet = styleSheet;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    // text and index are unknown until source load
    this.text = null;
    this._styleSheetIndex = -1;

    // When the style is imported, `styleSheet.ownerNode` is null,
    // so retrieve the topmost parent style sheet which has an ownerNode
    let parentStyleSheet = styleSheet;
    while (parentStyleSheet.parentStyleSheet) {
      parentStyleSheet = parentStyleSheet.parentStyleSheet;
    }
    // When the style is injected via nsIDOMWindowUtils.loadSheet, even
    // the parent style sheet has no owner, so default back to tab actor
    // document
    if (parentStyleSheet.ownerNode) {
      this.ownerDocument = parentStyleSheet.ownerNode.ownerDocument;
    } else {
      this.ownerDocument = parentActor.window;
    }
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
      styleSheetIndex: this.styleSheetIndex,
      sourceMapURL: this.rawSheet.sourceMapURL,
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
    // Check if network monitor observed this load, and if so, use that.
    let result = this.fetchStylesheetFromNetworkMonitor(href);
    if (result) {
      return result;
    }

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
      options.window = this.ownerWindow;
      options.principal = this.ownerDocument.nodePrincipal;
    }

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
   * Try to locate the console actor if it exists via our parent actor (the tab).
   */
  get _consoleActor() {
    if (this.parentActor.exited) {
      return null;
    }
    let form = this.parentActor.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  /**
   * Try to fetch the stylesheet text from the network monitor.  If it was enabled during
   * the load, it should have a copy of the text saved.
   *
   * @param string href
   *        The URL of the sheet to fetch.
   */
  fetchStylesheetFromNetworkMonitor(href) {
    let consoleActor = this._consoleActor;
    if (!consoleActor) {
      return null;
    }
    let request = consoleActor.getNetworkEventActorForURL(href);
    if (!request) {
      return null;
    }
    let content = request._response.content;
    if (request._discardResponseBody || !content) {
      return null;
    }
    if (content.text.type != "longString") {
      // For short strings, the text is available directly.
      return {
        content: content.text,
        contentType: content.mimeType,
      };
    }
    // For long strings, look up the actor that holds the full text.
    let longStringActor = this.conn._getOrCreateActor(content.text.actor);
    if (!longStringActor) {
      return null;
    }
    return {
      content: longStringActor.rawValue(),
      contentType: content.mimeType,
    };
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
      this._startTransition(kind);
    } else {
      this.emit("style-applied", kind, this);
    }

    this._getMediaRules().then((rules) => {
      this.emit("media-rules-changed", rules);
    });
  },

  /**
   * Insert a catch-all transition sheet into the document. Set a timeout
   * to remove the transition after a certain time.
   */
  _startTransition: function (kind) {
    if (!this._transitionSheetLoaded) {
      this._transitionSheetLoaded = true;
      // We don't remove this sheet. It uses an internal selector that
      // we only apply via locks, so there's no need to load and unload
      // it all the time.
      loadSheet(this.window, TRANSITION_SHEET);
    }

    addPseudoClassLock(this.document.documentElement, TRANSITION_PSEUDO_CLASS);

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

    this._onNewStyleSheetActor = this._onNewStyleSheetActor.bind(this);
    this._onSheetAdded = this._onSheetAdded.bind(this);
    this._onWindowReady = this._onWindowReady.bind(this);
    this._transitionSheetLoaded = false;

    this.parentActor.on("stylesheet-added", this._onNewStyleSheetActor);
    this.parentActor.on("window-ready", this._onWindowReady);

    // We listen for StyleSheetApplicableStateChanged rather than
    // StyleSheetAdded, because the latter will be sent before the
    // rules are ready.  Using the former (with a check to ensure that
    // the sheet is enabled) ensures that the sheet is ready before we
    // try to make an actor for it.
    this.parentActor.chromeEventHandler
      .addEventListener("StyleSheetApplicableStateChanged", this._onSheetAdded, true);

    // This is used when creating a new style sheet, so that we can
    // pass the correct flag when emitting our stylesheet-added event.
    // See addStyleSheet and _onNewStyleSheetActor for more details.
    this._nextStyleSheetIsNew = false;
  },

  destroy: function () {
    for (let win of this.parentActor.windows) {
      // This flag only exists for devtools, so we are free to clear
      // it when we're done.
      win.document.styleSheetChangeEventsEnabled = false;
    }

    this.parentActor.off("stylesheet-added", this._onNewStyleSheetActor);
    this.parentActor.off("window-ready", this._onWindowReady);

    this.parentActor.chromeEventHandler.removeEventListener("StyleSheetAdded",
                                                            this._onSheetAdded, true);

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Event handler that is called when a the tab actor emits window-ready.
   *
   * @param {Event} evt
   *        The triggering event.
   */
  _onWindowReady: function (evt) {
    this._addStyleSheets(evt.window);
  },

  /**
   * Event handler that is called when a the tab actor emits stylesheet-added.
   *
   * @param {StyleSheetActor} actor
   *        The new style sheet actor.
   */
  _onNewStyleSheetActor: function (actor) {
    // Forward it to the client side.
    this.emit("stylesheet-added", actor, this._nextStyleSheetIsNew);
    this._nextStyleSheetIsNew = false;
  },

  /**
   * Protocol method for getting a list of StyleSheetActors representing
   * all the style sheets in this document.
   */
  getStyleSheets: Task.async(function* () {
    let actors = [];

    for (let win of this.parentActor.windows) {
      let sheets = yield this._addStyleSheets(win);
      actors = actors.concat(sheets);
    }
    return actors;
  }),

  /**
   * Check if we should be showing this stylesheet.
   *
   * @param {DOMCSSStyleSheet} sheet
   *        Stylesheet we're interested in
   *
   * @return boolean
   *         Whether the stylesheet should be listed.
   */
  _shouldListSheet: function (sheet) {
    // Special case about:PreferenceStyleSheet, as it is generated on the
    // fly and the URI is not registered with the about: handler.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
    if (sheet.href && sheet.href.toLowerCase() == "about:preferencestylesheet") {
      return false;
    }

    return true;
  },

  /**
   * Event handler that is called when a new style sheet is added to
   * a document.  In particular,  StyleSheetApplicableStateChanged is
   * listened for, because StyleSheetAdded is sent too early, before
   * the rules are ready.
   *
   * @param {Event} evt
   *        The triggering event.
   */
  _onSheetAdded: function (evt) {
    let sheet = evt.stylesheet;
    if (this._shouldListSheet(sheet) && !this._haveAncestorWithSameURL(sheet)) {
      this.parentActor.createStyleSheetActor(sheet);
    }
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
      // We have to set this flag in order to get the
      // StyleSheetApplicableStateChanged events.  See Document.webidl.
      doc.styleSheetChangeEventsEnabled = true;

      let isChrome = Services.scriptSecurityManager.isSystemPrincipal(doc.nodePrincipal);
      let styleSheets = isChrome ? DOMUtils.getAllStyleSheets(doc) : doc.styleSheets;
      let actors = [];
      for (let i = 0; i < styleSheets.length; i++) {
        let sheet = styleSheets[i];
        if (!this._shouldListSheet(sheet)) {
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
              !this._shouldListSheet(sheet)) {
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
    // This is a bit convoluted.  The style sheet actor may be created
    // by a notification from platform.  In this case, we can't easily
    // pass the "new" flag through to createStyleSheetActor, so we set
    // a flag locally and check it before sending an event to the
    // client.  See |_onNewStyleSheetActor|.
    this._nextStyleSheetIsNew = true;

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
