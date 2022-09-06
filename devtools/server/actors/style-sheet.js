/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const protocol = require("devtools/shared/protocol");
const { LongStringActor } = require("devtools/server/actors/string");
const { MediaRuleActor } = require("devtools/server/actors/media-rule");
const { fetch } = require("devtools/shared/DevToolsUtils");
const { styleSheetSpec } = require("devtools/shared/specs/style-sheet");
const InspectorUtils = require("InspectorUtils");
const {
  getSourcemapBaseURL,
} = require("devtools/server/actors/utils/source-map-utils");

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

var TRANSITION_PSEUDO_CLASS = ":-moz-styleeditor-transitioning";
var TRANSITION_DURATION_MS = 500;
var TRANSITION_BUFFER_MS = 1000;
var TRANSITION_RULE_SELECTOR = `:root${TRANSITION_PSEUDO_CLASS}, :root${TRANSITION_PSEUDO_CLASS} *`;

var TRANSITION_SHEET =
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
exports.UPDATE_PRESERVING_RULES = UPDATE_PRESERVING_RULES;
const UPDATE_GENERAL = 1;
exports.UPDATE_GENERAL = UPDATE_GENERAL;

// If the user edits a style sheet, we stash a copy of the edited text
// here, keyed by the style sheet.  This way, if the tools are closed
// and then reopened, the edited text will be available.  A weak map
// is used so that navigation by the user will eventually cause the
// edited text to be collected.
const modifiedStyleSheets = new WeakMap();

function getSheetText(sheet) {
  const cssText = modifiedStyleSheets.get(sheet);
  if (cssText !== undefined) {
    return Promise.resolve(cssText);
  }

  if (!sheet.href) {
    // this is an inline <style> sheet
    const content = sheet.ownerNode.textContent;
    return Promise.resolve(content);
  }

  return fetchStylesheet(sheet).then(({ content }) => content);
}

exports.getSheetText = getSheetText;

/**
 * For imported stylesheets, `ownerNode` is null.
 * To resolve the ownerNode for an imported stylesheet, loop on `parentStylesheet`
 * until we reach the topmost stylesheet, which should have a valid ownerNode.
 *
 * @param {StyleSheet}
 *        The stylesheet for which we want to retrieve the ownerNode.
 * @return {DOMNode} The ownerNode
 */
function getSheetOwnerNode(sheet) {
  // If this is not an imported stylesheet and we have an ownerNode available
  // bail out immediately.
  if (sheet.ownerNode) {
    return sheet.ownerNode;
  }

  let parentStyleSheet = sheet;
  while (
    parentStyleSheet.parentStyleSheet &&
    parentStyleSheet !== parentStyleSheet.parentStyleSheet
  ) {
    parentStyleSheet = parentStyleSheet.parentStyleSheet;
  }

  return parentStyleSheet.ownerNode;
}
exports.getSheetOwnerNode = getSheetOwnerNode;

/**
 * Get the charset of the stylesheet.
 */
function getCSSCharset(sheet) {
  if (sheet) {
    // charset attribute of <link> or <style> element, if it exists
    if (sheet.ownerNode?.getAttribute) {
      const linkCharset = sheet.ownerNode.getAttribute("charset");
      if (linkCharset != null) {
        return linkCharset;
      }
    }

    // charset of referring document.
    if (sheet.ownerNode?.ownerDocument.characterSet) {
      return sheet.ownerNode.ownerDocument.characterSet;
    }
  }

  return "UTF-8";
}

/**
 * Fetch a stylesheet at the provided URL. Returns a promise that will resolve the
 * result of the fetch command.
 *
 * @return {Promise} a promise that resolves with an object with the following members
 *         on success:
 *           - content: the document at that URL, as a string,
 *           - contentType: the content type of the document
 *         If an error occurs, the promise is rejected with that error.
 */
async function fetchStylesheet(sheet) {
  const href = sheet.href;

  const options = {
    loadFromCache: true,
    policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
    charset: getCSSCharset(sheet),
  };

  // Bug 1282660 - We use the system principal to load the default internal
  // stylesheets instead of the content principal since such stylesheets
  // require system principal to load. At meanwhile, we strip the loadGroup
  // for preventing the assertion of the userContextId mismatching.

  // chrome|file|resource|moz-extension protocols rely on the system principal.
  const excludedProtocolsRe = /^(chrome|file|resource|moz-extension):\/\//;
  if (!excludedProtocolsRe.test(href)) {
    // Stylesheets using other protocols should use the content principal.
    const ownerNode = getSheetOwnerNode(sheet);
    if (ownerNode) {
      // eslint-disable-next-line mozilla/use-ownerGlobal
      options.window = ownerNode.ownerDocument.defaultView;
      options.principal = ownerNode.ownerDocument.nodePrincipal;
    }
  }

  let result;

  try {
    result = await fetch(href, options);
  } catch (e) {
    // The list of excluded protocols can be missing some protocols, try to use the
    // system principal if the first fetch failed.
    console.error(
      `stylesheets actor: fetch failed for ${href},` +
        ` using system principal instead.`
    );
    options.window = undefined;
    options.principal = undefined;
    result = await fetch(href, options);
  }

  return result;
}

/**
 * A StyleSheetActor represents a stylesheet on the server.
 */
var StyleSheetActor = protocol.ActorClassWithSpec(styleSheetSpec, {
  toString() {
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
      if (this.ownerNode.nodeType == this.ownerNode.DOCUMENT_NODE) {
        href = this.ownerNode.location.href;
      } else if (
        this.ownerNode.ownerDocument &&
        this.ownerNode.ownerDocument.location
      ) {
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
      const styleSheets = InspectorUtils.getAllStyleSheets(this.document, true);
      for (let i = 0; i < styleSheets.length; i++) {
        if (styleSheets[i] == this.rawSheet) {
          this._styleSheetIndex = i;
          break;
        }
      }
    }
    return this._styleSheetIndex;
  },

  destroy() {
    if (this._transitionTimeout && this.window) {
      this.window.clearTimeout(this._transitionTimeout);
      removePseudoClassLock(
        this.document.documentElement,
        TRANSITION_PSEUDO_CLASS
      );
    }
    protocol.Actor.prototype.destroy.call(this);
  },

  initialize(styleSheet, parentActor) {
    protocol.Actor.prototype.initialize.call(this, parentActor.conn);

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
    // the parent style sheet has no owner, so default back to target actor
    // document
    if (parentStyleSheet.ownerNode) {
      this.ownerDocument = parentStyleSheet.ownerNode.ownerDocument;
    } else {
      this.ownerDocument = parentActor.window;
    }
  },

  /**
   * Test whether this sheet has been modified by CSSOM.
   * @return {Boolean} true if changed by CSSOM.
   */
  hasRulesModifiedByCSSOM() {
    return InspectorUtils.hasRulesModifiedByCSSOM(this.rawSheet);
  },

  /**
   * Get the raw stylesheet's cssRules once the sheet has been loaded.
   *
   * @return {Promise}
   *         Promise that resolves with a CSSRuleList
   */
  getCSSRules() {
    let rules;
    try {
      rules = this.rawSheet.cssRules;
    } catch (e) {
      // sheet isn't loaded yet
    }

    if (rules) {
      return Promise.resolve(rules);
    }

    if (!this.ownerNode) {
      return Promise.resolve([]);
    }

    if (this._cssRules) {
      return this._cssRules;
    }

    // cache so we don't add many listeners if this is called multiple times.
    this._cssRules = new Promise(resolve => {
      const onSheetLoaded = event => {
        this.ownerNode.removeEventListener("load", onSheetLoaded);

        resolve(this.rawSheet.cssRules);
      };

      this.ownerNode.addEventListener("load", onSheetLoaded);
    });

    return this._cssRules;
  },

  /**
   * Get the current state of the actor
   *
   * @return {object}
   *         With properties of the underlying stylesheet, plus 'text',
   *        'styleSheetIndex' and 'parentActor' if it's @imported
   */
  form() {
    let docHref;
    if (this.ownerNode) {
      if (this.ownerNode.nodeType == this.ownerNode.DOCUMENT_NODE) {
        docHref = this.ownerNode.location.href;
      } else if (
        this.ownerNode.ownerDocument &&
        this.ownerNode.ownerDocument.location
      ) {
        docHref = this.ownerNode.ownerDocument.location.href;
      }
    }

    const form = {
      actor: this.actorID, // actorID is set when this actor is added to a pool
      href: this.href,
      nodeHref: docHref,
      disabled: this.rawSheet.disabled,
      constructed: this.rawSheet.constructed,
      title: this.rawSheet.title,
      system: CssLogic.isAgentStylesheet(this.rawSheet),
      styleSheetIndex: this.styleSheetIndex,
      sourceMapBaseURL: getSourcemapBaseURL(
        // Technically resolveSourceURL should be used here alongside
        // "this.rawSheet.sourceURL", but the style inspector does not support
        // /*# sourceURL=*/ in CSS, so we're omitting it here (bug 880831).
        this.href || docHref,
        this.ownerWindow
      ),
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
  toggleDisabled() {
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
  _notifyPropertyChanged(property) {
    this.emit("property-change", property, this.form()[property]);
  },

  /**
   * Protocol method to get the text of this stylesheet.
   */
  getText() {
    return this._getText().then(text => {
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
  _getText() {
    if (typeof this.text === "string") {
      return Promise.resolve(this.text);
    }

    return getSheetText(this.rawSheet).then(text => {
      this.text = text;
      return text;
    });
  },

  /**
   * Protocol method to get the media rules for the stylesheet.
   */
  getMediaRules() {
    return this._getMediaRules();
  },

  /**
   * Get all the @media rules in this stylesheet.
   *
   * @return {promise}
   *         A promise that resolves with an array of MediaRuleActors.
   */
  _getMediaRules() {
    const mediaRules = [];
    const traverseRules = ruleList => {
      for (const rule of ruleList) {
        if (rule.type === CSSRule.MEDIA_RULE) {
          const actor = new MediaRuleActor(rule, this);
          this.manage(actor);
          mediaRules.push(actor);
        }

        if (rule.cssRules) {
          traverseRules(rule.cssRules);
        }
      }
    };

    return this.getCSSRules().then(rules => {
      traverseRules(rules);
      return mediaRules;
    });
  },

  /**
   * Update the style sheet in place with new text.
   *
   * @param {string} text: new text
   * @param {boolean} transition: whether to do CSS transition for change.
   * @param {string} kind: either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   * @param {string|null} cause: indicates the cause of this update
   */
  update(text, transition, kind = UPDATE_GENERAL, cause) {
    InspectorUtils.parseStyleSheet(this.rawSheet, text);

    modifiedStyleSheets.set(this.rawSheet, text);

    this.text = text;

    if (kind != UPDATE_PRESERVING_RULES) {
      this._notifyPropertyChanged("ruleCount");
    }

    if (transition) {
      this._startTransition(kind, cause);
    } else {
      this.emit("style-applied", kind, this, cause);
    }

    this._getMediaRules().then(rules => {
      this.emit("media-rules-changed", rules);
    });
  },

  /**
   * Insert a catch-all transition sheet into the document. Set a timeout
   * to remove the transition after a certain time.
   *
   * @param {string} kind: either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   * @param {string|null} cause: indicates the cause of this update
   */
  _startTransition(kind, cause) {
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
      this._onTransitionEnd.bind(this, kind, cause),
      TRANSITION_DURATION_MS + TRANSITION_BUFFER_MS
    );
  },

  /**
   * This cleans up class and rule added for transition effect and then
   * notifies that the style has been applied.
   *
   * @param {string} kind: either UPDATE_PRESERVING_RULES or UPDATE_GENERAL
   * @param {string|null} cause: indicates the cause of this update
   */
  _onTransitionEnd(kind, cause) {
    this._transitionTimeout = null;
    removePseudoClassLock(
      this.document.documentElement,
      TRANSITION_PSEUDO_CLASS
    );
    this.emit("style-applied", kind, this, cause);
  },
});

exports.StyleSheetActor = StyleSheetActor;
