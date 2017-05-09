/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const promise = require("promise");
const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");
const {fetch} = require("devtools/shared/DevToolsUtils");
const {oldStyleSheetSpec, styleEditorSpec} = require("devtools/shared/specs/styleeditor");

loader.lazyGetter(this, "CssLogic", () => require("devtools/shared/inspector/css-logic"));

var TRANSITION_CLASS = "moz-styleeditor-transitioning";
var TRANSITION_DURATION_MS = 500;
var TRANSITION_RULE = ":root.moz-styleeditor-transitioning, " +
                      ":root.moz-styleeditor-transitioning * {\n" +
                        "transition-duration: " + TRANSITION_DURATION_MS +
                          "ms !important;\n" +
                        "transition-delay: 0ms !important;\n" +
                        "transition-timing-function: ease-out !important;\n" +
                        "transition-property: all !important;\n" +
                      "}";

var OldStyleSheetActor = protocol.ActorClassWithSpec(oldStyleSheetSpec, {
  toString: function () {
    return "[OldStyleSheetActor " + this.actorID + "]";
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

  /**
   * URL of underlying stylesheet.
   */
  get href() {
    return this.rawSheet.href;
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

  initialize: function (styleSheet, parentActor, window) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.rawSheet = styleSheet;
    this.parentActor = parentActor;
    this.conn = this.parentActor.conn;

    this._window = window;

    // text and index are unknown until source load
    this.text = null;
    this._styleSheetIndex = -1;

    this._transitionRefCount = 0;

    // if this sheet has an @import, then it's rules are loaded async
    let ownerNode = this.rawSheet.ownerNode;
    if (ownerNode) {
      let onSheetLoaded = (event) => {
        ownerNode.removeEventListener("load", onSheetLoaded);
        this._notifyPropertyChanged("ruleCount");
      };

      ownerNode.addEventListener("load", onSheetLoaded);
    }
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
    if (this.rawSheet.ownerNode) {
      if (this.rawSheet.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
        docHref = this.rawSheet.ownerNode.location.href;
      }
      if (this.rawSheet.ownerNode.ownerDocument) {
        docHref = this.rawSheet.ownerNode.ownerDocument.location.href;
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
    events.emit(this, "property-change", property, this.form()[property]);
  },

   /**
    * Fetch the source of the style sheet from its URL. Send a "sourceLoad"
    * event when it's been fetched.
    */
  fetchSource: function () {
    this._getText().then((content) => {
      events.emit(this, "source-load", this.text);
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
    if (this.text) {
      return promise.resolve(this.text);
    }

    if (!this.href) {
      // this is an inline <style> sheet
      let content = this.rawSheet.ownerNode.textContent;
      this.text = content;
      return promise.resolve(content);
    }

    let options = {
      policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
      window: this.window,
      charset: this._getCSSCharset()
    };

    return fetch(this.href, options).then(({ content }) => {
      this.text = content;
      return content;
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
   */
  update: function (text, transition) {
    DOMUtils.parseStyleSheet(this.rawSheet, text);

    this.text = text;

    this._notifyPropertyChanged("ruleCount");

    if (transition) {
      this._insertTransistionRule();
    } else {
      this._notifyStyleApplied();
    }
  },

  /**
   * Insert a catch-all transition rule into the document. Set a timeout
   * to remove the rule after a certain time.
   */
  _insertTransistionRule: function () {
    // Insert the global transition rule
    // Use a ref count to make sure we do not add it multiple times.. and remove
    // it only when all pending StyleEditor-generated transitions ended.
    if (this._transitionRefCount == 0) {
      this.rawSheet.insertRule(TRANSITION_RULE, this.rawSheet.cssRules.length);
      this.document.documentElement.classList.add(TRANSITION_CLASS);
    }

    this._transitionRefCount++;

    // Set up clean up and commit after transition duration (+10% buffer)
    // @see _onTransitionEnd
    this.window.setTimeout(this._onTransitionEnd.bind(this),
                           Math.floor(TRANSITION_DURATION_MS * 1.1));
  },

  /**
    * This cleans up class and rule added for transition effect and then
    * notifies that the style has been applied.
    */
  _onTransitionEnd: function () {
    if (--this._transitionRefCount == 0) {
      this.document.documentElement.classList.remove(TRANSITION_CLASS);
      this.rawSheet.deleteRule(this.rawSheet.cssRules.length - 1);
    }

    events.emit(this, "style-applied");
  }
});

exports.OldStyleSheetActor = OldStyleSheetActor;

/**
 * Creates a StyleEditorActor. StyleEditorActor provides remote access to the
 * stylesheets of a document.
 */
var StyleEditorActor = protocol.ActorClassWithSpec(styleEditorSpec, {
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

    // keep a map of sheets-to-actors so we don't create two actors for one sheet
    this._sheets = new Map();
  },

  /**
   * Destroy the current StyleEditorActor instance.
   */
  destroy: function () {
    this._sheets.clear();
  },

  /**
   * Called by client when target navigates to a new document.
   * Adds load listeners to document.
   */
  newDocument: function () {
    // delete previous document's actors
    this._clearStyleSheetActors();

    // Note: listening for load won't be necessary once
    // https://bugzilla.mozilla.org/show_bug.cgi?id=839103 is fixed
    if (this.document.readyState == "complete") {
      this._onDocumentLoaded();
    } else {
      this.window.addEventListener("load", this._onDocumentLoaded);
    }
    return {};
  },

  /**
   * Event handler for document loaded event. Add actor for each stylesheet
   * and send an event notifying of the load
   */
  _onDocumentLoaded: function (event) {
    if (event) {
      this.window.removeEventListener("load", this._onDocumentLoaded);
    }

    let documents = [this.document];
    let forms = [];
    for (let doc of documents) {
      let sheetForms = this._addStyleSheets(doc.styleSheets);
      forms = forms.concat(sheetForms);
      // Recursively handle style sheets of the documents in iframes.
      for (let iframe of doc.getElementsByTagName("iframe")) {
        documents.push(iframe.contentDocument);
      }
    }

    events.emit(this, "document-load", forms);
  },

  /**
   * Add all the stylesheets to the map and create an actor for each one
   * if not already created. Send event that there are new stylesheets.
   *
   * @param {[DOMStyleSheet]} styleSheets
   *        Stylesheets to add
   * @return {[object]}
   *         Array of actors for each StyleSheetActor created
   */
  _addStyleSheets: function (styleSheets) {
    let sheets = [];
    for (let i = 0; i < styleSheets.length; i++) {
      let styleSheet = styleSheets[i];
      sheets.push(styleSheet);

      // Get all sheets, including imported ones
      let imports = this._getImported(styleSheet);
      sheets = sheets.concat(imports);
    }
    let actors = sheets.map(this._createStyleSheetActor.bind(this));

    return actors;
  },

  /**
   * Create a new actor for a style sheet, if it hasn't already been created.
   *
   * @param  {DOMStyleSheet} styleSheet
   *         The style sheet to create an actor for.
   * @return {StyleSheetActor}
   *         The actor for this style sheet
   */
  _createStyleSheetActor: function (styleSheet) {
    if (this._sheets.has(styleSheet)) {
      return this._sheets.get(styleSheet);
    }
    let actor = new OldStyleSheetActor(styleSheet, this);

    this.manage(actor);
    this._sheets.set(styleSheet, actor);

    return actor;
  },

  /**
   * Get all the stylesheets @imported from a stylesheet.
   *
   * @param  {DOMStyleSheet} styleSheet
   *         Style sheet to search
   * @return {array}
   *         All the imported stylesheets
   */
  _getImported: function (styleSheet) {
    let imported = [];

    for (let i = 0; i < styleSheet.cssRules.length; i++) {
      let rule = styleSheet.cssRules[i];
      if (rule.type == Ci.nsIDOMCSSRule.IMPORT_RULE) {
        // Associated styleSheet may be null if it has already been seen due to
        // duplicate @imports for the same URL.
        if (!rule.styleSheet) {
          continue;
        }
        imported.push(rule.styleSheet);

        // recurse imports in this stylesheet as well
        imported = imported.concat(this._getImported(rule.styleSheet));
      } else if (rule.type != Ci.nsIDOMCSSRule.CHARSET_RULE) {
        // @import rules must precede all others except @charset
        break;
      }
    }
    return imported;
  },

  /**
   * Clear all the current stylesheet actors in map.
   */
  _clearStyleSheetActors: function () {
    for (let actor in this._sheets) {
      this.unmanage(this._sheets[actor]);
    }
    this._sheets.clear();
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
  newStyleSheet: function (text) {
    let parent = this.document.documentElement;
    let style = this.document.createElementNS("http://www.w3.org/1999/xhtml", "style");
    style.setAttribute("type", "text/css");

    if (text) {
      style.appendChild(this.document.createTextNode(text));
    }
    parent.appendChild(style);

    let actor = this._createStyleSheetActor(style.sheet);
    return actor;
  }
});

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

exports.StyleEditorActor = StyleEditorActor;

