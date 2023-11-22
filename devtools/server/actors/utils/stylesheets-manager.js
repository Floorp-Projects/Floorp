/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const {
  getSourcemapBaseURL,
} = require("resource://devtools/server/actors/utils/source-map-utils.js");

loader.lazyRequireGetter(
  this,
  ["addPseudoClassLock", "removePseudoClassLock"],
  "resource://devtools/server/actors/highlighters/utils/markup.js",
  true
);
loader.lazyRequireGetter(
  this,
  "loadSheet",
  "resource://devtools/shared/layout/utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  ["getStyleSheetOwnerNode", "getStyleSheetText"],
  "resource://devtools/server/actors/utils/stylesheet-utils.js",
  true
);

const TRANSITION_PSEUDO_CLASS = ":-moz-styleeditor-transitioning";
const TRANSITION_DURATION_MS = 500;
const TRANSITION_BUFFER_MS = 1000;
const TRANSITION_RULE_SELECTOR = `:root${TRANSITION_PSEUDO_CLASS}, :root${TRANSITION_PSEUDO_CLASS} *:not(:-moz-native-anonymous)`;
const TRANSITION_SHEET =
  "data:text/css;charset=utf-8," +
  encodeURIComponent(`
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
const UPDATE_GENERAL = 1;

// If the user edits a stylesheet, we stash a copy of the edited text
// here, keyed by the stylesheet.  This way, if the tools are closed
// and then reopened, the edited text will be available. A weak map
// is used so that navigation by the user will eventually cause the
// edited text to be collected.
const modifiedStyleSheets = new WeakMap();

/**
 * Manage stylesheets related to a given Target Actor.
 *
 * @emits applicable-stylesheet-added: emitted when an applicable stylesheet is added to the document.
 *        First arg is an object with the following properties:
 *        - resourceId {String}: The id that was assigned to the stylesheet
 *        - styleSheet {StyleSheet}: The actual stylesheet
 *        - creationData {Object}: An object with:
 *            - isCreatedByDevTools {Boolean}: Was the stylesheet created by DevTools (e.g.
 *              by the user clicking the new stylesheet button in the styleeditor)
 *            - fileName {String}
 * @emits stylesheet-updated: emitted when there was changes in a stylesheet
 *        First arg is an object with the following properties:
 *        - resourceId {String}: The id that was assigned to the stylesheet
 *        - updateKind {String}: Which kind of update it is ("style-applied",
 *          "at-rules-changed", "matches-change", "property-change")
 *        - updates {Object}: The update data
 */
class StyleSheetsManager extends EventEmitter {
  _styleSheetCount = 0;
  _styleSheetMap = new Map();
  // List of all watched media queries. Change listeners are being registered from getStyleSheetRuleCountAndAtRules.
  _mqlList = [];

  /**
   * @param TargetActor targetActor
   *        The target actor from which we should observe stylesheet changes.
   */
  constructor(targetActor) {
    super();

    this._targetActor = targetActor;
    this._onApplicableStateChanged = this._onApplicableStateChanged.bind(this);
    this._onStylesheetRemoved = this._onStylesheetRemoved.bind(this);
    this._onTargetActorWindowReady = this._onTargetActorWindowReady.bind(this);
  }

  /**
   * Calling this function will make the StyleSheetsManager start the event listeners needed
   * to watch for stylesheet additions and modifications.
   * It will also trigger applicable-stylesheet-added events for the existing stylesheets.
   * This function resolves once it notified about existing stylesheets.
   */
  async startWatching() {
    // Process existing stylesheets
    const promises = [];
    for (const window of this._targetActor.windows) {
      promises.push(this._getStyleSheetsForWindow(window));
    }

    // Listen for new stylesheet being added via StyleSheetApplicableStateChanged
    this._targetActor.chromeEventHandler.addEventListener(
      "StyleSheetApplicableStateChanged",
      this._onApplicableStateChanged,
      true
    );
    this._targetActor.chromeEventHandler.addEventListener(
      "StyleSheetRemoved",
      this._onStylesheetRemoved,
      true
    );
    this._watchStyleSheetChangeEvents();
    this._targetActor.on("window-ready", this._onTargetActorWindowReady);

    // Finally, notify about existing stylesheets
    let styleSheets = await Promise.all(promises);
    styleSheets = styleSheets.flat();
    for (const styleSheet of styleSheets) {
      const resourceId = this._findStyleSheetResourceId(styleSheet);
      if (resourceId) {
        // If the stylesheet was already registered before any consumer started
        // watching, emit "applicable-stylesheet-added" immediately.
        this.emitAsync("applicable-stylesheet-added", {
          resourceId,
          styleSheet,
        });
      } else {
        this._registerStyleSheet(styleSheet);
      }
    }
  }

  _watchStyleSheetChangeEvents() {
    for (const window of this._targetActor.windows) {
      this._watchStyleSheetChangeEventsForWindow(window);
    }
  }

  _onTargetActorWindowReady({ window }) {
    this._watchStyleSheetChangeEventsForWindow(window);
  }

  _watchStyleSheetChangeEventsForWindow(window) {
    // We have to set this flag in order to get the
    // StyleSheetApplicableStateChanged and StyleSheetRemoved events. See Document.webidl.
    window.document.styleSheetChangeEventsEnabled = true;
  }

  _unwatchStyleSheetChangeEvents() {
    for (const window of this._targetActor.windows) {
      window.document.styleSheetChangeEventsEnabled = false;
    }
  }

  /**
   * Create a new style sheet in the document with the given text.
   *
   * @param  {Document} document
   *         Document that the new style sheet belong to.
   * @param  {string} text
   *         Content of style sheet.
   * @param  {string} fileName
   *         If the stylesheet adding is from file, `fileName` indicates the path.
   */
  async addStyleSheet(document, text, fileName) {
    const parent = document.documentElement;
    const style = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "style"
    );
    style.setAttribute("type", "text/css");
    style.setDevtoolsAsTriggeringPrincipal();

    if (text) {
      style.appendChild(document.createTextNode(text));
    }

    // This triggers StyleSheetApplicableStateChanged event.
    parent.appendChild(style);

    // This promise will be resolved when the resource for this stylesheet is available.
    let resolve = null;
    const promise = new Promise(r => {
      resolve = r;
    });

    if (!this._styleSheetCreationData) {
      this._styleSheetCreationData = new WeakMap();
    }
    this._styleSheetCreationData.set(style.sheet, {
      isCreatedByDevTools: true,
      fileName,
      resolve,
    });

    await promise;

    return style.sheet;
  }

  /**
   * Return resourceId of the given style sheet or create one if the stylesheet wasn't
   * registered yet.
   *
   * @params {StyleSheet} styleSheet
   * @returns {String} resourceId
   */
  getStyleSheetResourceId(styleSheet) {
    const existingResourceId = this._findStyleSheetResourceId(styleSheet);
    if (existingResourceId) {
      return existingResourceId;
    }

    // If we couldn't find an associated resourceId, that means the stylesheet isn't
    // registered yet. Calling _registerStyleSheet will register it and return the
    // associated resourceId it computed for it.
    return this._registerStyleSheet(styleSheet);
  }

  /**
   * Return the associated resourceId of the given registered style sheet, or null if the
   * stylesheet wasn't registered yet.
   *
   * @params {StyleSheet} styleSheet
   * @returns {String} resourceId
   */
  _findStyleSheetResourceId(styleSheet) {
    for (const [
      resourceId,
      existingStyleSheet,
    ] of this._styleSheetMap.entries()) {
      if (styleSheet === existingStyleSheet) {
        return resourceId;
      }
    }

    return null;
  }

  /**
   * Return owner node of the style sheet of the given resource id.
   *
   * @params {String} resourceId
   *                  The id associated with the stylesheet
   * @returns {Element|null}
   */
  getOwnerNode(resourceId) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    return styleSheet.ownerNode;
  }

  /**
   * Return the index of given stylesheet of the given resource id.
   *
   * @params {String} resourceId
   *                  The id associated with the stylesheet
   * @returns {Number}
   */
  getStyleSheetIndex(resourceId) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    return this._getStyleSheetIndex(styleSheet);
  }

  /**
   * Get the text of a stylesheet given its resourceId.
   *
   * @params {String} resourceId
   *                  The id associated with the stylesheet
   * @returns {String}
   */
  async getText(resourceId) {
    const styleSheet = this._styleSheetMap.get(resourceId);

    const modifiedText = modifiedStyleSheets.get(styleSheet);

    // modifiedText is the content of the stylesheet updated by update function.
    // In case not updating, this is undefined.
    if (modifiedText !== undefined) {
      return modifiedText;
    }

    return getStyleSheetText(styleSheet);
  }

  /**
   * Toggle the disabled property of the stylesheet
   *
   * @params {String} resourceId
   *                  The id associated with the stylesheet
   * @return {Boolean} the disabled state after toggling.
   */
  toggleDisabled(resourceId) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    styleSheet.disabled = !styleSheet.disabled;

    this._notifyPropertyChanged(resourceId, "disabled", styleSheet.disabled);

    return styleSheet.disabled;
  }

  /**
   * Update the style sheet in place with new text.
   *
   * @param  {String} resourceId
   * @param  {String} text
   *         New text.
   * @param  {Object} options
   * @param  {Boolean} options.transition
   *         Whether to do CSS transition for change. Defaults to false.
   * @param  {Number} options.kind
   *         Either UPDATE_PRESERVING_RULES or UPDATE_GENERAL. Defaults to UPDATE_GENERAL.
   * @param {String} options.cause
   *         Indicates the cause of this update (e.g. "styleeditor") if this was called
   *         from the stylesheet to be edited by the user from the StyleEditor.
   */
  async setStyleSheetText(
    resourceId,
    text,
    { transition = false, kind = UPDATE_GENERAL, cause = "" } = {}
  ) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    InspectorUtils.parseStyleSheet(styleSheet, text);
    modifiedStyleSheets.set(styleSheet, text);

    // Remove event handler from all media query list we set to. We are going to re-set
    // those handler properly from getStyleSheetRuleCountAndAtRules.
    for (const mql of this._mqlList) {
      mql.onchange = null;
    }

    const { atRules, ruleCount } =
      this.getStyleSheetRuleCountAndAtRules(styleSheet);

    if (kind !== UPDATE_PRESERVING_RULES) {
      this._notifyPropertyChanged(resourceId, "ruleCount", ruleCount);
    }

    if (transition) {
      this._startTransition(resourceId, kind, cause);
    } else {
      this.emit("stylesheet-updated", {
        resourceId,
        updateKind: "style-applied",
        updates: {
          event: { kind, cause },
        },
      });
    }

    this.emit("stylesheet-updated", {
      resourceId,
      updateKind: "at-rules-changed",
      updates: {
        resourceUpdates: { atRules },
      },
    });
  }

  /**
   * Applies a transition to the stylesheet document so any change made by the user in the
   * client will be animated so it's more visible.
   *
   * @param {String} resourceId
   *        The id associated with the stylesheet
   * @param {Number} kind
   *        Either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   * @param {String} cause
   *         Indicates the cause of this update (e.g. "styleeditor") if this was called
   *         from the stylesheet to be edited by the user from the StyleEditor.
   */
  _startTransition(resourceId, kind, cause) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    const document = styleSheet.associatedDocument;
    const window = document.ownerGlobal;

    if (!this._transitionSheetLoaded) {
      this._transitionSheetLoaded = true;
      // We don't remove this sheet. It uses an internal selector that
      // we only apply via locks, so there's no need to load and unload
      // it all the time.
      loadSheet(window, TRANSITION_SHEET);
    }

    addPseudoClassLock(document.documentElement, TRANSITION_PSEUDO_CLASS);

    // Set up clean up and commit after transition duration (+buffer)
    // @see _onTransitionEnd
    window.clearTimeout(this._transitionTimeout);
    this._transitionTimeout = window.setTimeout(
      this._onTransitionEnd.bind(this, resourceId, kind, cause),
      TRANSITION_DURATION_MS + TRANSITION_BUFFER_MS
    );
  }

  /**
   * @param {String} resourceId
   *        The id associated with the stylesheet
   * @param {Number} kind
   *        Either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   * @param {String} cause
   *         Indicates the cause of this update (e.g. "styleeditor") if this was called
   *         from the stylesheet to be edited by the user from the StyleEditor.
   */
  _onTransitionEnd(resourceId, kind, cause) {
    const styleSheet = this._styleSheetMap.get(resourceId);
    const document = styleSheet.associatedDocument;

    this._transitionTimeout = null;
    removePseudoClassLock(document.documentElement, TRANSITION_PSEUDO_CLASS);

    this.emit("stylesheet-updated", {
      resourceId,
      updateKind: "style-applied",
      updates: {
        event: { kind, cause },
      },
    });
  }

  /**
   * Retrieve the CSSRuleList of a given stylesheet
   *
   * @param {StyleSheet} styleSheet
   * @returns {CSSRuleList}
   */
  _getCSSRules(styleSheet) {
    try {
      return styleSheet.cssRules;
    } catch (e) {
      // sheet isn't loaded yet
    }

    if (!styleSheet.ownerNode) {
      return Promise.resolve([]);
    }

    return new Promise(resolve => {
      styleSheet.ownerNode.addEventListener(
        "load",
        () => resolve(styleSheet.cssRules),
        { once: true }
      );
    });
  }

  /**
   * Get the stylesheets imported by a given stylesheet (via @import)
   *
   * @param {Document} document
   * @param {StyleSheet} styleSheet
   * @returns Array<StyleSheet>
   */
  async _getImportedStyleSheets(document, styleSheet) {
    const importedStyleSheets = [];

    for (const rule of await this._getCSSRules(styleSheet)) {
      const ruleClassName = ChromeUtils.getClassName(rule);
      if (ruleClassName == "CSSImportRule") {
        // With the Gecko style system, the associated styleSheet may be null
        // if it has already been seen because an import cycle for the same
        // URL.  With Stylo, the styleSheet will exist (which is correct per
        // the latest CSSOM spec), so we also need to check ancestors for the
        // same URL to avoid cycles.
        if (
          !rule.styleSheet ||
          this._haveAncestorWithSameURL(rule.styleSheet) ||
          !this._shouldListSheet(rule.styleSheet)
        ) {
          continue;
        }

        importedStyleSheets.push(rule.styleSheet);

        // recurse imports in this stylesheet as well
        const children = await this._getImportedStyleSheets(
          document,
          rule.styleSheet
        );
        importedStyleSheets.push(...children);
      } else if (ruleClassName != "CSSCharsetRule") {
        // @import rules must precede all others except @charset
        break;
      }
    }

    return importedStyleSheets;
  }

  /**
   * Retrieve the total number of rules (including nested ones) and
   * all the at-rules of a given stylesheet.
   *
   * @param {StyleSheet} styleSheet
   * @returns {Object} An object of the following shape:
   *          - {Integer} ruleCount: The total number of rules in the stylesheet
   *          - {Array<Object>} atRules: An array of object of the following shape:
   *            - type {String}
   *            - conditionText {String}
   *            - matches {Boolean}: true if the media rule matches the current state of the document
   *            - layerName {String}
   *            - line {Number}
   *            - column {Number}
   */
  getStyleSheetRuleCountAndAtRules(styleSheet) {
    const resourceId = this._findStyleSheetResourceId(styleSheet);
    if (!resourceId) {
      return [];
    }

    this._mqlList = [];

    // Accessing the stylesheet associated window might be slow due to cross compartment
    // wrappers, so only retrieve it if it's needed.
    let win;
    const getStyleSheetAssociatedWindow = () => {
      if (!win) {
        win = styleSheet.associatedDocument?.ownerGlobal;
      }
      return win;
    };

    const styleSheetRules =
      InspectorUtils.getAllStyleSheetCSSStyleRules(styleSheet);
    const ruleCount = styleSheetRules.length;
    // We need to go through nested rules to extract all the rules we're interested in
    const atRules = [];
    for (const rule of styleSheetRules) {
      const className = ChromeUtils.getClassName(rule);
      if (className === "CSSMediaRule") {
        let matches = false;

        try {
          const mql = getStyleSheetAssociatedWindow().matchMedia(
            rule.media.mediaText
          );
          matches = mql.matches;
          mql.onchange = this._onMatchesChange.bind(
            this,
            resourceId,
            atRules.length
          );
          this._mqlList.push(mql);
        } catch (e) {
          // Ignored
        }

        atRules.push({
          type: "media",
          conditionText: rule.conditionText,
          matches,
          line: InspectorUtils.getRelativeRuleLine(rule),
          column: InspectorUtils.getRuleColumn(rule),
        });
      } else if (className === "CSSContainerRule") {
        atRules.push({
          type: "container",
          conditionText: rule.conditionText,
          line: InspectorUtils.getRelativeRuleLine(rule),
          column: InspectorUtils.getRuleColumn(rule),
        });
      } else if (className === "CSSSupportsRule") {
        atRules.push({
          type: "support",
          conditionText: rule.conditionText,
          line: InspectorUtils.getRelativeRuleLine(rule),
          column: InspectorUtils.getRuleColumn(rule),
        });
      } else if (className === "CSSLayerBlockRule") {
        atRules.push({
          type: "layer",
          layerName: rule.name,
          line: InspectorUtils.getRelativeRuleLine(rule),
          column: InspectorUtils.getRuleColumn(rule),
        });
      }
    }
    return { ruleCount, atRules };
  }

  /**
   * Called when the status of a media query support changes (i.e. it now matches, or it
   * was matching but isn't anymore)
   *
   * @param {String} resourceId
   *        The id associated with the stylesheet
   * @param {Number} index
   *        The index of the media rule relatively to all the other at-rules of the stylesheet
   * @param {MediaQueryList} mql
   *        The result of matchMedia for the given media rule
   */
  _onMatchesChange(resourceId, index, mql) {
    this.emit("stylesheet-updated", {
      resourceId,
      updateKind: "matches-change",
      updates: {
        nestedResourceUpdates: [
          {
            path: ["atRules", index, "matches"],
            value: mql.matches,
          },
        ],
      },
    });
  }

  /**
   * Get the node href of a given stylesheet
   *
   * @param {StyleSheet} styleSheet
   * @returns {String}
   */
  _getNodeHref(styleSheet) {
    const { ownerNode } = styleSheet;
    if (!ownerNode) {
      return null;
    }

    if (ownerNode.nodeType == ownerNode.DOCUMENT_NODE) {
      return ownerNode.location.href;
    }

    if (ownerNode.ownerDocument?.location) {
      return ownerNode.ownerDocument.location.href;
    }

    return null;
  }

  /**
   * Get the sourcemap base url of a given stylesheet
   *
   * @param {StyleSheet} styleSheet
   * @returns {String}
   */
  _getSourcemapBaseURL(styleSheet) {
    // When the style is injected via nsIDOMWindowUtils.loadSheet, even
    // the parent style sheet has no owner, so default back to target actor
    // document
    const ownerNode = getStyleSheetOwnerNode(styleSheet);
    const ownerDocument = ownerNode
      ? ownerNode.ownerDocument
      : this._targetActor.window;

    return getSourcemapBaseURL(
      // Technically resolveSourceURL should be used here alongside
      // "this.rawSheet.sourceURL", but the style inspector does not support
      // /*# sourceURL=*/ in CSS, so we're omitting it here (bug 880831).
      styleSheet.href || this._getNodeHref(styleSheet),
      ownerDocument
    );
  }

  /**
   * Get the index of a given stylesheet in the document it lives in
   *
   * @param {StyleSheet} styleSheet
   * @returns {Number}
   */
  _getStyleSheetIndex(styleSheet) {
    const styleSheets = InspectorUtils.getAllStyleSheets(
      this._targetActor.window.document,
      true
    );
    let i = 0;
    for (const sheet of styleSheets) {
      if (!this._shouldListSheet(sheet)) {
        continue;
      }
      if (sheet == styleSheet) {
        return i;
      }
      i++;
    }
    return -1;
  }

  /**
   * Get all the stylesheets for a given window
   *
   * @param {Window} window
   * @returns {Array<StyleSheet>}
   */
  async _getStyleSheetsForWindow(window) {
    const { document } = window;
    const documentOnly = !document.nodePrincipal.isSystemPrincipal;

    const styleSheets = [];

    for (const styleSheet of InspectorUtils.getAllStyleSheets(
      document,
      documentOnly
    )) {
      if (!this._shouldListSheet(styleSheet)) {
        continue;
      }

      styleSheets.push(styleSheet);

      // Get all sheets, including imported ones
      const importedStyleSheets = await this._getImportedStyleSheets(
        document,
        styleSheet
      );
      styleSheets.push(...importedStyleSheets);
    }

    return styleSheets;
  }

  /**
   * Returns true if a given stylesheet has an ancestor with the same url it has
   *
   * @param {StyleSheet} styleSheet
   * @returns {Boolean}
   */
  _haveAncestorWithSameURL(styleSheet) {
    const href = styleSheet.href;
    while (styleSheet.parentStyleSheet) {
      if (styleSheet.parentStyleSheet.href == href) {
        return true;
      }
      styleSheet = styleSheet.parentStyleSheet;
    }
    return false;
  }

  /**
   * Helper function called when a property changed in a given stylesheet
   *
   * @param {String} resourceId
   *        The id of the stylesheet the change occured in
   * @param {String} property
   *        The property that was changed
   * @param {String} value
   *        The value of the property
   */
  _notifyPropertyChanged(resourceId, property, value) {
    this.emit("stylesheet-updated", {
      resourceId,
      updateKind: "property-change",
      updates: { resourceUpdates: { [property]: value } },
    });
  }

  /**
   * Event handler that is called when the state of applicable of style sheet is changed.
   *
   * For now, StyleSheetApplicableStateChanged event will be called at following timings.
   * - Append <link> of stylesheet to document
   * - Append <style> to document
   * - Change disable attribute of stylesheet object
   * - Change disable attribute of <link> to false
   * - Stylesheet is constructed.
   * When appending <link>, <style> or changing `disabled` attribute to false,
   * `applicable` is passed as true. The other hand, when changing `disabled`
   * to true, this will be false.
   *
   * NOTE: StyleSheetApplicableStateChanged is _not_ called when removing the <link>/<style>,
   *       but a StyleSheetRemovedEvent is emitted in such case (see _onStyleSheetRemoved)
   *
   * @param {StyleSheetApplicableStateChangedEvent}
   *        The triggering event.
   */
  _onApplicableStateChanged({ applicable, stylesheet: styleSheet }) {
    if (
      // Have interest in applicable stylesheet only.
      applicable &&
      styleSheet.associatedDocument &&
      (!this._targetActor.ignoreSubFrames ||
        styleSheet.associatedDocument.ownerGlobal ===
          this._targetActor.window) &&
      this._shouldListSheet(styleSheet) &&
      !this._haveAncestorWithSameURL(styleSheet)
    ) {
      this._registerStyleSheet(styleSheet);
    }
  }

  /**
   * Event handler that is called when a style sheet is removed.
   *
   * @param {StyleSheetRemovedEvent}
   *        The triggering event.
   */
  _onStylesheetRemoved(event) {
    this._unregisterStyleSheet(event.stylesheet);
  }

  /**
   * If the stylesheet isn't registered yet, this function will generate an associated
   * resourceId and will emit an "applicable-stylesheet-added" event.
   *
   * @param {StyleSheet} styleSheet
   * @returns {String} the associated resourceId
   */
  _registerStyleSheet(styleSheet) {
    const existingResourceId = this._findStyleSheetResourceId(styleSheet);
    // If the stylesheet is already registered, there's no need to notify about it again.
    if (existingResourceId) {
      return existingResourceId;
    }

    // It's important to prefix the resourceId with the target actorID so we can't have
    // duplicated resource ids when the client connects to multiple targets.
    const resourceId = `${this._targetActor.actorID}:stylesheet:${this
      ._styleSheetCount++}`;
    this._styleSheetMap.set(resourceId, styleSheet);

    const creationData = this._styleSheetCreationData?.get(styleSheet);
    this._styleSheetCreationData?.delete(styleSheet);

    // We need to use emitAsync and await on it so the watcher can sends the resource to
    // the client before we resolve a potential creationData promise.
    const onEventHandlerDone = this.emitAsync("applicable-stylesheet-added", {
      resourceId,
      styleSheet,
      creationData,
    });

    // creationData exists if this stylesheet was created via `addStyleSheet`.
    if (creationData) {
      //  We resolve the promise once the handler of applicable-stylesheet-added are settled,
      // (e.g. the watcher sent the resources to the client) so `addStyleSheet` calls can
      // be fullfilled.
      onEventHandlerDone.then(() => creationData?.resolve());
    }
    return resourceId;
  }

  /**
   * If the stylesheet is registered, this function will emit an "applicable-stylesheet-removed" event
   * with the stylesheet resourceId.
   *
   * @param {StyleSheet} styleSheet
   */
  _unregisterStyleSheet(styleSheet) {
    const existingResourceId = this._findStyleSheetResourceId(styleSheet);
    if (!existingResourceId) {
      return;
    }

    this._styleSheetMap.delete(existingResourceId);
    this._styleSheetCreationData?.delete(styleSheet);
    this.emit("applicable-stylesheet-removed", {
      resourceId: existingResourceId,
    });
  }

  /**
   * Returns true if the passed styleSheet should be handled.
   *
   * @param {StyleSheet} styleSheet
   * @returns {Boolean}
   */
  _shouldListSheet(styleSheet) {
    const href = styleSheet.href?.toLowerCase();
    // FIXME(bug 1826538): Make accessiblecaret.css and similar UA-widget
    // sheets system sheets, then remove this special-case.
    if (
      href === "resource://content-accessible/accessiblecaret.css" ||
      (href === "resource://devtools-highlighter-styles/highlighters.css" &&
        this._targetActor.sessionContext.type !== "all")
    ) {
      return false;
    }
    return true;
  }

  /**
   * The StyleSheetsManager instance is managed by the target, so this will be called when
   * the target gets destroyed.
   */
  destroy() {
    // Cleanup
    this._targetActor.off("window-ready", this._watchStyleSheetChangeEvents);

    try {
      this._targetActor.chromeEventHandler.removeEventListener(
        "StyleSheetApplicableStateChanged",
        this._onApplicableStateChanged,
        true
      );
      this._targetActor.chromeEventHandler.removeEventListener(
        "StyleSheetRemoved",
        this._onStylesheetRemoved,
        true
      );
      this._unwatchStyleSheetChangeEvents();
    } catch (e) {
      console.error(
        "Error when destroying StyleSheet manager for",
        this._targetActor,
        ": ",
        e
      );
    }

    this._styleSheetMap.clear();
    this._styleSheetMap = null;
    this._targetActor = null;
    this._styleSheetCreationData = null;
    this._mqlList = null;
  }
}

module.exports = {
  StyleSheetsManager,
  UPDATE_GENERAL,
  UPDATE_PRESERVING_RULES,
};
