/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { fetch } = require("devtools/shared/DevToolsUtils");
const InspectorUtils = require("InspectorUtils");
const {
  getSourcemapBaseURL,
} = require("devtools/server/actors/utils/source-map-utils");

const {
  TYPES: { STYLESHEET },
} = require("devtools/server/actors/resources/index");

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/shared/inspector/css-logic"
);

loader.lazyRequireGetter(
  this,
  ["addPseudoClassLock", "removePseudoClassLock"],
  "devtools/server/actors/highlighters/utils/markup",
  true
);
loader.lazyRequireGetter(
  this,
  "loadSheet",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  ["UPDATE_GENERAL", "UPDATE_PRESERVING_RULES"],
  "devtools/server/actors/style-sheet",
  true
);

const TRANSITION_PSEUDO_CLASS = ":-moz-styleeditor-transitioning";
const TRANSITION_DURATION_MS = 500;
const TRANSITION_BUFFER_MS = 1000;
const TRANSITION_RULE_SELECTOR = `:root${TRANSITION_PSEUDO_CLASS}, :root${TRANSITION_PSEUDO_CLASS} *`;
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

// If the user edits a stylesheet, we stash a copy of the edited text
// here, keyed by the stylesheet.  This way, if the tools are closed
// and then reopened, the edited text will be available. A weak map
// is used so that navigation by the user will eventually cause the
// edited text to be collected.
const modifiedStyleSheets = new WeakMap();

class StyleSheetWatcher {
  constructor() {
    this._resourceCount = 0;
    // The _styleSheetMap maps resourceId and following value.
    // {
    //   styleSheet: Raw StyleSheet object.
    // }
    this._styleSheetMap = new Map();
    // List of all watched media queries. Change listeners are being registered from _getMediaRules.
    this._mqlList = [];

    this._onApplicableStateChanged = this._onApplicableStateChanged.bind(this);
  }

  /**
   * Start watching for all stylesheets related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe css changes.
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable, onUpdated }) {
    this._targetActor = targetActor;
    this._onAvailable = onAvailable;
    this._onUpdated = onUpdated;

    // Listen for new stylesheet being added via StyleSheetApplicableStateChanged
    this._targetActor.chromeEventHandler.addEventListener(
      "StyleSheetApplicableStateChanged",
      this._onApplicableStateChanged,
      true
    );

    const styleSheets = [];

    for (const window of this._targetActor.windows) {
      // We have to set this flag in order to get the
      // StyleSheetApplicableStateChanged events.  See Document.webidl.
      window.document.styleSheetChangeEventsEnabled = true;

      styleSheets.push(...(await this._getStyleSheets(window)));
    }

    await this._notifyResourcesAvailable(styleSheets);
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

    if (!this._stylesheetCreationData) {
      this._stylesheetCreationData = new WeakMap();
    }
    this._stylesheetCreationData.set(style.sheet, {
      isCreatedByDevTools: true,
      fileName,
      resolve,
    });

    await promise;

    return style.sheet;
  }

  async ensureResourceAvailable(styleSheet) {
    if (this.getResourceId(styleSheet)) {
      return;
    }

    await this._notifyResourcesAvailable([styleSheet]);
  }

  /**
   * Return resourceId of the given style sheet.
   */
  getResourceId(styleSheet) {
    for (const [resourceId, value] of this._styleSheetMap.entries()) {
      if (styleSheet === value.styleSheet) {
        return resourceId;
      }
    }

    return null;
  }

  /**
   * Return owner node of the style sheet of the given resource id.
   */
  getOwnerNode(resourceId) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    return styleSheet.ownerNode;
  }

  /**
   * Return the style sheet of the given resource id.
   */
  getStyleSheet(resourceId) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    return styleSheet;
  }

  /**
   * Return the index of given stylesheet of the given resource id.
   */
  getStyleSheetIndex(resourceId) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    return this._getStyleSheetIndex(styleSheet);
  }

  /**
   * Protocol method to get the text of stylesheet of resourceId.
   */
  async getText(resourceId) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);

    const modifiedText = modifiedStyleSheets.get(styleSheet);

    // modifiedText is the content of the stylesheet updated by update function.
    // In case not updating, this is undefined.
    if (modifiedText !== undefined) {
      return modifiedText;
    }

    if (!styleSheet.href) {
      // this is an inline <style> sheet
      return styleSheet.ownerNode.textContent;
    }

    return this._fetchStylesheet(styleSheet);
  }

  /**
   * Toggle the disabled property of the stylesheet
   *
   * @return {Boolean} the disabled state after toggling.
   */
  toggleDisabled(resourceId) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
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
   * @param  {Boolean} transition
   *         Whether to do CSS transition for change.
   * @param  {Number} kind
   *         Either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   */
  async update(resourceId, text, transition, kind = UPDATE_GENERAL) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    InspectorUtils.parseStyleSheet(styleSheet, text);
    modifiedStyleSheets.set(styleSheet, text);

    if (kind !== UPDATE_PRESERVING_RULES) {
      this._notifyPropertyChanged(
        resourceId,
        "ruleCount",
        styleSheet.cssRules.length
      );
    }

    if (transition) {
      this._startTransition(resourceId, kind);
    } else {
      this._notifyResourceUpdated(resourceId, "style-applied", {
        event: { kind },
      });
    }

    // Remove event handler from all media query list we set to.
    for (const mql of this._mqlList) {
      mql.onchange = null;
    }

    const mediaRules = await this._getMediaRules(resourceId, styleSheet);
    this._notifyResourceUpdated(resourceId, "media-rules-changed", {
      resourceUpdates: { mediaRules },
    });
  }

  _startTransition(resourceId, kind) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    const document = styleSheet.ownerNode.ownerDocument;
    const window = styleSheet.ownerNode.ownerGlobal;

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
      this._onTransitionEnd.bind(this, resourceId, kind),
      TRANSITION_DURATION_MS + TRANSITION_BUFFER_MS
    );
  }

  _onTransitionEnd(resourceId, kind) {
    const { styleSheet } = this._styleSheetMap.get(resourceId);
    const document = styleSheet.ownerNode.ownerDocument;

    this._transitionTimeout = null;
    removePseudoClassLock(document.documentElement, TRANSITION_PSEUDO_CLASS);
    this._notifyResourceUpdated(resourceId, "style-applied", {
      event: { kind },
    });
  }

  async _fetchStylesheet(styleSheet) {
    const href = styleSheet.href;

    const options = {
      loadFromCache: true,
      policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
      charset: this._getCSSCharset(styleSheet),
    };

    // Bug 1282660 - We use the system principal to load the default internal
    // stylesheets instead of the content principal since such stylesheets
    // require system principal to load. At meanwhile, we strip the loadGroup
    // for preventing the assertion of the userContextId mismatching.

    // chrome|file|resource|moz-extension protocols rely on the system principal.
    const excludedProtocolsRe = /^(chrome|file|resource|moz-extension):\/\//;
    if (!excludedProtocolsRe.test(href)) {
      // Stylesheets using other protocols should use the content principal.
      if (styleSheet.ownerNode) {
        // eslint-disable-next-line mozilla/use-ownerGlobal
        options.window = styleSheet.ownerNode.ownerDocument.defaultView;
        options.principal = styleSheet.ownerNode.ownerDocument.nodePrincipal;
      }
    }

    let result;

    try {
      result = await fetch(href, options);
    } catch (e) {
      // The list of excluded protocols can be missing some protocols, try to use the
      // system principal if the first fetch failed.
      console.error(
        `stylesheets: fetch failed for ${href},` +
          ` using system principal instead.`
      );
      options.window = undefined;
      options.principal = undefined;
      result = await fetch(href, options);
    }

    return result.content;
  }

  _getCSSCharset(styleSheet) {
    if (styleSheet) {
      // charset attribute of <link> or <style> element, if it exists
      if (styleSheet.ownerNode?.getAttribute) {
        const linkCharset = styleSheet.ownerNode.getAttribute("charset");
        if (linkCharset != null) {
          return linkCharset;
        }
      }

      // charset of referring document.
      if (styleSheet.ownerNode?.ownerDocument.characterSet) {
        return styleSheet.ownerNode.ownerDocument.characterSet;
      }
    }

    return "UTF-8";
  }

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

  async _getImportedStyleSheets(document, styleSheet) {
    const importedStyleSheets = [];

    for (const rule of await this._getCSSRules(styleSheet)) {
      if (rule.type == CSSRule.IMPORT_RULE) {
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
      } else if (rule.type != CSSRule.CHARSET_RULE) {
        // @import rules must precede all others except @charset
        break;
      }
    }

    return importedStyleSheets;
  }

  async _getMediaRules(resourceId, styleSheet) {
    this._mqlList = [];

    const mediaRules = Array.from(await this._getCSSRules(styleSheet)).filter(
      rule => rule.type === CSSRule.MEDIA_RULE
    );

    return mediaRules.map((rule, index) => {
      let matches = false;

      try {
        const window = styleSheet.ownerNode.ownerGlobal;
        const mql = window.matchMedia(rule.media.mediaText);
        matches = mql.matches;
        mql.onchange = this._onMatchesChange.bind(this, resourceId, index);
        this._mqlList.push(mql);
      } catch (e) {
        // Ignored
      }

      return {
        mediaText: rule.media.mediaText,
        conditionText: rule.conditionText,
        matches,
        line: InspectorUtils.getRuleLine(rule),
        column: InspectorUtils.getRuleColumn(rule),
      };
    });
  }

  _onMatchesChange(resourceId, index, mql) {
    this._notifyResourceUpdated(resourceId, "matches-change", {
      nestedResourceUpdates: [
        {
          path: ["mediaRules", index, "matches"],
          value: mql.matches,
        },
      ],
    });
  }

  _getNodeHref(styleSheet) {
    const { ownerNode } = styleSheet;
    if (!ownerNode) {
      return null;
    }

    if (ownerNode.nodeType == ownerNode.DOCUMENT_NODE) {
      return ownerNode.location.href;
    } else if (ownerNode.ownerDocument?.location) {
      return ownerNode.ownerDocument.location.href;
    }

    return null;
  }

  _getSourcemapBaseURL(styleSheet) {
    // When the style is imported, `styleSheet.ownerNode` is null,
    // so retrieve the topmost parent style sheet which has an ownerNode
    let parentStyleSheet = styleSheet;
    while (parentStyleSheet.parentStyleSheet) {
      parentStyleSheet = parentStyleSheet.parentStyleSheet;
    }

    // When the style is injected via nsIDOMWindowUtils.loadSheet, even
    // the parent style sheet has no owner, so default back to target actor
    // document
    const ownerDocument = parentStyleSheet.ownerNode
      ? parentStyleSheet.ownerNode.ownerDocument
      : this._targetActor.window;

    return getSourcemapBaseURL(
      // Technically resolveSourceURL should be used here alongside
      // "this.rawSheet.sourceURL", but the style inspector does not support
      // /*# sourceURL=*/ in CSS, so we're omitting it here (bug 880831).
      styleSheet.href || this._getNodeHref(styleSheet),
      ownerDocument
    );
  }

  _getStyleSheetIndex(styleSheet) {
    const styleSheets = InspectorUtils.getAllStyleSheets(
      this._targetActor.window.document,
      true
    );
    return styleSheets.indexOf(styleSheet);
  }

  async _getStyleSheets({ document }) {
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

  _notifyPropertyChanged(resourceId, property, value) {
    this._notifyResourceUpdated(resourceId, "property-change", {
      resourceUpdates: { [property]: value },
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
   * When appending <link>, <style> or changing `disable` attribute to false, `applicable`
   * is passed as true. The other hand, when changing `disable` to true, this will be
   * false.
   * NOTE: For now, StyleSheetApplicableStateChanged will not be called when removing the
   *       link and style element.
   *
   * @param {StyleSheetApplicableStateChanged}
   *        The triggering event.
   */
  async _onApplicableStateChanged({ applicable, stylesheet: styleSheet }) {
    for (const existing of this._styleSheetMap.values()) {
      if (existing.styleSheet === styleSheet) {
        return;
      }
    }

    if (
      // Have interest in applicable stylesheet only.
      applicable &&
      // No ownerNode means that this stylesheet is *not* associated to a DOM Element.
      styleSheet.ownerNode &&
      this._shouldListSheet(styleSheet) &&
      !this._haveAncestorWithSameURL(styleSheet)
    ) {
      await this._notifyResourcesAvailable([styleSheet]);
    }
  }

  _shouldListSheet(styleSheet) {
    // Special case about:PreferenceStyleSheet, as it is generated on the
    // fly and the URI is not registered with the about: handler.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
    if (styleSheet.href?.toLowerCase() === "about:preferencestylesheet") {
      return false;
    }

    return true;
  }

  async _toResource(
    styleSheet,
    { isCreatedByDevTools = false, fileName = null } = {}
  ) {
    const resourceId = `stylesheet:${this._resourceCount++}`;

    const resource = {
      resourceId,
      resourceType: STYLESHEET,
      disabled: styleSheet.disabled,
      fileName,
      href: styleSheet.href,
      isNew: isCreatedByDevTools,
      mediaRules: await this._getMediaRules(resourceId, styleSheet),
      nodeHref: this._getNodeHref(styleSheet),
      ruleCount: styleSheet.cssRules.length,
      sourceMapBaseURL: this._getSourcemapBaseURL(styleSheet),
      sourceMapURL: styleSheet.sourceMapURL,
      styleSheetIndex: this._getStyleSheetIndex(styleSheet),
      system: CssLogic.isAgentStylesheet(styleSheet),
      title: styleSheet.title,
    };

    return resource;
  }

  async _notifyResourcesAvailable(styleSheets) {
    const resources = await Promise.all(
      styleSheets.map(async styleSheet => {
        const creationData = this._stylesheetCreationData?.get(styleSheet);

        const resource = await this._toResource(styleSheet, {
          isCreatedByDevTools: creationData?.isCreatedByDevTools,
          fileName: creationData?.fileName,
        });

        this._styleSheetMap.set(resource.resourceId, { styleSheet });
        return resource;
      })
    );

    await this._onAvailable(resources);

    for (const styleSheet of styleSheets) {
      const creationData = this._stylesheetCreationData?.get(styleSheet);
      creationData?.resolve();
      this._stylesheetCreationData?.delete(styleSheet);
    }
  }

  _notifyResourceUpdated(
    resourceId,
    updateType,
    { resourceUpdates, nestedResourceUpdates, event }
  ) {
    this._onUpdated([
      {
        resourceType: STYLESHEET,
        resourceId,
        updateType,
        resourceUpdates,
        nestedResourceUpdates,
        event,
      },
    ]);
  }

  destroy() {
    if (!this._targetActor.docShell) {
      return;
    }

    this._targetActor.chromeEventHandler.removeEventListener(
      "StyleSheetApplicableStateChanged",
      this._onApplicableStateChanged,
      true
    );
  }
}

module.exports = StyleSheetWatcher;
