/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { LongStringActor } = require("devtools/server/actors/string");
const { styleSheetsSpec } = require("devtools/shared/specs/style-sheets");
const InspectorUtils = require("InspectorUtils");

loader.lazyRequireGetter(
  this,
  "UPDATE_GENERAL",
  "devtools/server/actors/style-sheet",
  true
);
loader.lazyRequireGetter(
  this,
  "hasStyleSheetWatcherSupportForTarget",
  "devtools/server/actors/utils/stylesheets-manager",
  true
);

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

  initialize(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, targetActor.conn);

    this.parentActor = targetActor;

    this._onApplicableStateChanged = this._onApplicableStateChanged.bind(this);
    this._onNewStyleSheetActor = this._onNewStyleSheetActor.bind(this);
    this._onWindowReady = this._onWindowReady.bind(this);
    this._transitionSheetLoaded = false;

    this.parentActor.on("stylesheet-added", this._onNewStyleSheetActor);
    this.parentActor.on("window-ready", this._onWindowReady);

    this.parentActor.chromeEventHandler.addEventListener(
      "StyleSheetApplicableStateChanged",
      this._onApplicableStateChanged,
      true
    );
  },

  getTraits() {
    return {
      traits: {},
    };
  },

  destroy() {
    for (const win of this.parentActor.windows) {
      // This flag only exists for devtools, so we are free to clear
      // it when we're done.
      win.document.styleSheetChangeEventsEnabled = false;
    }

    this.parentActor.off("stylesheet-added", this._onNewStyleSheetActor);
    this.parentActor.off("window-ready", this._onWindowReady);

    this.parentActor.chromeEventHandler.removeEventListener(
      "StyleSheetApplicableStateChanged",
      this._onApplicableStateChanged,
      true
    );

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Event handler that is called when a the target actor emits window-ready.
   *
   * @param {Event} evt
   *        The triggering event.
   */
  _onWindowReady(evt) {
    this._addStyleSheets(evt.window);
  },

  /**
   * Event handler that is called when a the target actor emits stylesheet-added.
   *
   * @param {StyleSheetActor} actor
   *        The new style sheet actor.
   */
  _onNewStyleSheetActor(actor) {
    const info = this._addingStyleSheetInfo?.get(actor.rawSheet);
    this._addingStyleSheetInfo?.delete(actor.rawSheet);

    // Forward it to the client side.
    this.emit(
      "stylesheet-added",
      actor,
      info ? info.isNew : false,
      info ? info.fileName : null
    );
  },

  /**
   * Protocol method for getting a list of StyleSheetActors representing
   * all the style sheets in this document.
   */
  async getStyleSheets() {
    let actors = [];

    const windows = this.parentActor.windows;
    for (const win of windows) {
      const sheets = await this._addStyleSheets(win);
      actors = actors.concat(sheets);
    }
    return actors;
  },

  /**
   * Check if we should be showing this stylesheet.
   *
   * @param {DOMCSSStyleSheet} sheet
   *        Stylesheet we're interested in
   *
   * @return boolean
   *         Whether the stylesheet should be listed.
   */
  _shouldListSheet(sheet) {
    // Special case about:PreferenceStyleSheet, as it is generated on the
    // fly and the URI is not registered with the about: handler.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
    if (sheet.href?.toLowerCase() === "about:preferencestylesheet") {
      return false;
    }

    return true;
  },

  /**
   * Event handler that is called when the state of applicable of style sheet is changed.
   *
   * For now, StyleSheetApplicableStateChanged event will be called at following timings.
   * - Append <link> of stylesheet to document
   * - Append <style> to document
   * - Change disable attribute of stylesheet object
   * - Change disable attribute of <link> to false
   * When appending <link>, <style> or changing `disable` attribute to false, `applicable`
   * is passed as true. The other hand, when changing `disable` to true, this will be
   * false.
   * NOTE: For now, StyleSheetApplicableStateChanged will not be called when removing the
   *       link and style element.
   *
   * @param {StyleSheetApplicableStateChanged}
   *        The triggering event.
   */
  _onApplicableStateChanged({ applicable, stylesheet }) {
    if (
      // Have interest in applicable stylesheet only.
      applicable &&
      // No ownerNode means that this stylesheet is *not* associated to a DOM Element.
      stylesheet.ownerNode &&
      this._shouldListSheet(stylesheet) &&
      !this._haveAncestorWithSameURL(stylesheet)
    ) {
      this.parentActor.createStyleSheetActor(stylesheet);
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
  _addStyleSheets(win) {
    return async function() {
      const doc = win.document;
      // We have to set this flag in order to get the
      // StyleSheetApplicableStateChanged events.  See Document.webidl.
      doc.styleSheetChangeEventsEnabled = true;

      const documentOnly = !doc.nodePrincipal.isSystemPrincipal;
      const styleSheets = InspectorUtils.getAllStyleSheets(doc, documentOnly);

      let actors = [];
      for (let i = 0; i < styleSheets.length; i++) {
        const sheet = styleSheets[i];
        if (!this._shouldListSheet(sheet)) {
          continue;
        }

        const actor = this.parentActor.createStyleSheetActor(sheet);
        actors.push(actor);

        // Get all sheets, including imported ones
        const imports = await this._getImported(doc, actor);
        actors = actors.concat(imports);
      }
      return actors;
    }.bind(this)();
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
  _getImported(doc, styleSheet) {
    return async function() {
      const rules = await styleSheet.getCSSRules();
      let imported = [];

      for (let i = 0; i < rules.length; i++) {
        const rule = rules[i];
        if (rule.type == CSSRule.IMPORT_RULE) {
          // With the Gecko style system, the associated styleSheet may be null
          // if it has already been seen because an import cycle for the same
          // URL.  With Stylo, the styleSheet will exist (which is correct per
          // the latest CSSOM spec), so we also need to check ancestors for the
          // same URL to avoid cycles.
          const sheet = rule.styleSheet;
          if (
            !sheet ||
            this._haveAncestorWithSameURL(sheet) ||
            !this._shouldListSheet(sheet)
          ) {
            continue;
          }
          const actor = this.parentActor.createStyleSheetActor(rule.styleSheet);
          imported.push(actor);

          // recurse imports in this stylesheet as well
          const children = await this._getImported(doc, actor);
          imported = imported.concat(children);
        } else if (rule.type != CSSRule.CHARSET_RULE) {
          // @import rules must precede all others except @charset
          break;
        }
      }

      return imported;
    }.bind(this)();
  },

  /**
   * Check all ancestors to see if this sheet's URL matches theirs as a way to
   * detect an import cycle.
   *
   * @param {DOMStyleSheet} sheet
   */
  _haveAncestorWithSameURL(sheet) {
    const sheetHref = sheet.href;
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
   * @param  {string} fileName
   *         If the stylesheet adding is from file, `fileName` indicates the path.
   * @return {object}
   *         Object with 'styelSheet' property for form on new actor.
   */
  async addStyleSheet(text, fileName = null) {
    if (this._hasStyleSheetWatcherSupport()) {
      const styleSheetsManager = this._getStyleSheetsManager();
      await styleSheetsManager.addStyleSheet(this.document, text, fileName);
      return;
    }

    // Following code can be removed once we enable STYLESHEET resource on the watcher/server
    // side by default. For now it is being preffed off and we have to support the two
    // codepaths. Once enabled we will only support the stylesheet watcher codepath.
    const parent = this.document.documentElement;
    const style = this.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "style"
    );
    style.setAttribute("type", "text/css");

    if (text) {
      style.appendChild(this.document.createTextNode(text));
    }
    parent.appendChild(style);

    // This is a bit convoluted.  The style sheet actor may be created
    // by a notification from platform.  In this case, we can't easily
    // pass the "new" flag through to createStyleSheetActor, so we set
    // a flag locally and check it before sending an event to the
    // client.  See |_onNewStyleSheetActor|.
    if (!this._addingStyleSheetInfo) {
      this._addingStyleSheetInfo = new WeakMap();
    }
    this._addingStyleSheetInfo.set(style.sheet, { isNew: true, fileName });

    const actor = this.parentActor.createStyleSheetActor(style.sheet);
    // eslint-disable-next-line consistent-return
    return actor;
  },

  _getStyleSheetActor(resourceId) {
    return this.parentActor._targetScopedActorPool.getActorByID(resourceId);
  },

  _hasStyleSheetWatcherSupport() {
    return hasStyleSheetWatcherSupportForTarget(this.parentActor);
  },

  _getStyleSheetsManager() {
    return this.parentActor.getStyleSheetManager();
  },

  toggleDisabled(resourceId) {
    if (this._hasStyleSheetWatcherSupport()) {
      const styleSheetsManager = this._getStyleSheetsManager();
      return styleSheetsManager.toggleDisabled(resourceId);
    }

    // Following code can be removed once we enable STYLESHEET resource on the watcher/server
    // side by default. For now it is being preffed off and we have to support the two
    // codepaths. Once enabled we will only support the stylesheet watcher codepath.
    const actor = this._getStyleSheetActor(resourceId);
    return actor.toggleDisabled();
  },

  async getText(resourceId) {
    if (this._hasStyleSheetWatcherSupport()) {
      const styleSheetsManager = this._getStyleSheetsManager();
      const text = await styleSheetsManager.getText(resourceId);
      return new LongStringActor(this.conn, text || "");
    }

    // Following code can be removed once we enable STYLESHEET resource on the watcher/server
    // side by default. For now it is being preffed off and we have to support the two
    // codepaths. Once enabled we will only support the stylesheet watcher codepath.
    const actor = this._getStyleSheetActor(resourceId);
    return actor.getText();
  },

  update(resourceId, text, transition, cause = "") {
    if (this._hasStyleSheetWatcherSupport()) {
      const styleSheetsManager = this._getStyleSheetsManager();
      return styleSheetsManager.setStyleSheetText(resourceId, text, {
        transition,
        kind: UPDATE_GENERAL,
        cause,
      });
    }

    // Following code can be removed once we enable STYLESHEET resource on the watcher/server
    // side by default. For now it is being preffed off and we have to support the two
    // codepaths. Once enabled we will only support the stylesheet watcher codepath.
    const actor = this._getStyleSheetActor(resourceId);
    return actor.update(text, transition, UPDATE_GENERAL, cause);
  },
});

exports.StyleSheetsActor = StyleSheetsActor;
