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

class StyleSheetWatcher {
  constructor() {
    this._resourceCount = 0;
    this._styleSheetMap = new Map();
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
    this._onUpdated = onUpdated;

    const styleSheets = [];

    for (const window of this._targetActor.windows) {
      // We have to set this flag in order to get the
      // StyleSheetApplicableStateChanged events.  See Document.webidl.
      window.document.styleSheetChangeEventsEnabled = true;

      styleSheets.push(...(await this._getStyleSheets(window)));
    }

    onAvailable(
      await Promise.all(
        styleSheets.map(styleSheet => this._toResource(styleSheet))
      )
    );
  }

  /**
   * Protocol method to get the text of stylesheet of resourceId.
   */
  async getText(resourceId) {
    const styleSheet = this._styleSheetMap.get(resourceId);

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
    const styleSheet = this._styleSheetMap.get(resourceId);
    styleSheet.disabled = !styleSheet.disabled;

    this._notifyPropertyChanged(resourceId, "disabled", styleSheet.disabled);

    return styleSheet.disabled;
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

  async _getMediaRules(styleSheet) {
    const mediaRules = Array.from(await this._getCSSRules(styleSheet)).filter(
      rule => rule.type === CSSRule.MEDIA_RULE
    );

    return mediaRules.map(rule => {
      let matches = false;

      try {
        const window = styleSheet.ownerNode.ownerGlobal;
        const mql = window.matchMedia(rule.media.mediaText);
        matches = mql.matches;
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
    this._updateResource(resourceId, "property-change", {
      [property]: value,
    });
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

  async _toResource(styleSheet) {
    const resource = {
      resourceId: `stylesheet:${this._resourceCount++}`,
      resourceType: STYLESHEET,
      disabled: styleSheet.disabled,
      href: styleSheet.href,
      mediaRules: await this._getMediaRules(styleSheet),
      nodeHref: this._getNodeHref(styleSheet),
      ruleCount: styleSheet.cssRules.length,
      sourceMapBaseURL: this._getSourcemapBaseURL(styleSheet),
      sourceMapURL: styleSheet.sourceMapURL,
      styleSheetIndex: this._getStyleSheetIndex(styleSheet),
      system: !CssLogic.isAuthorStylesheet(styleSheet),
      title: styleSheet.title,
    };

    this._styleSheetMap.set(resource.resourceId, styleSheet);

    return resource;
  }

  _updateResource(resourceId, updateType, resourceUpdates) {
    this._onUpdated([
      {
        resourceType: STYLESHEET,
        resourceId,
        updateType,
        resourceUpdates,
      },
    ]);
  }

  destroy() {}
}

module.exports = StyleSheetWatcher;
