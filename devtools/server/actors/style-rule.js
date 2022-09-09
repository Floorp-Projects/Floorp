/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { getCSSLexer } = require("devtools/shared/css/lexer");
const InspectorUtils = require("InspectorUtils");
const TrackChangeEmitter = require("devtools/server/actors/utils/track-change-emitter");
const {
  getRuleText,
  getTextAtLineColumn,
} = require("devtools/server/actors/utils/style-utils");

const { styleRuleSpec } = require("devtools/shared/specs/style-rule");
const {
  style: { ELEMENT_STYLE },
} = require("devtools/shared/constants");

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "SharedCssLogic",
  "devtools/shared/inspector/css-logic"
);
loader.lazyRequireGetter(
  this,
  ["CSSRuleTypeName", "findCssSelector", "prettifyCSS"],
  "devtools/shared/inspector/css-logic",
  true
);
loader.lazyRequireGetter(
  this,
  "isCssPropertyKnown",
  "devtools/server/actors/css-properties",
  true
);
loader.lazyRequireGetter(
  this,
  "isPropertyUsed",
  "devtools/server/actors/utils/inactive-property-helper",
  true
);
loader.lazyRequireGetter(
  this,
  "parseNamedDeclarations",
  "devtools/shared/css/parsing-utils",
  true
);
loader.lazyRequireGetter(
  this,
  ["UPDATE_PRESERVING_RULES", "UPDATE_GENERAL"],
  "devtools/server/actors/style-sheet",
  true
);

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const SUPPORTED_RULE_TYPES = [
  CSSRule.STYLE_RULE,
  CSSRule.SUPPORTS_RULE,
  CSSRule.KEYFRAME_RULE,
  CSSRule.KEYFRAMES_RULE,
  CSSRule.MEDIA_RULE,
];

/**
 * An actor that represents a CSS style object on the protocol.
 *
 * We slightly flatten the CSSOM for this actor, it represents
 * both the CSSRule and CSSStyle objects in one actor.  For nodes
 * (which have a CSSStyle but no CSSRule) we create a StyleRuleActor
 * with a special rule type (100).
 */
const StyleRuleActor = protocol.ActorClassWithSpec(styleRuleSpec, {
  initialize(pageStyle, item) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.pageStyle = pageStyle;
    this.rawStyle = item.style;
    this._parentSheet = null;
    this._onStyleApplied = this._onStyleApplied.bind(this);
    // Parsed CSS declarations from this.form().declarations used to check CSS property
    // names and values before tracking changes. Using cached values instead of accessing
    // this.form().declarations on demand because that would cause needless re-parsing.
    this._declarations = [];

    this._pendingDeclarationChanges = [];

    if (CSSRule.isInstance(item)) {
      this.type = item.type;
      this.rawRule = item;
      this._computeRuleIndex();
      if (
        SUPPORTED_RULE_TYPES.includes(this.type) &&
        this.rawRule.parentStyleSheet
      ) {
        this.line = InspectorUtils.getRelativeRuleLine(this.rawRule);
        this.column = InspectorUtils.getRuleColumn(this.rawRule);
        this._parentSheet = this.rawRule.parentStyleSheet;
        if (!this.pageStyle.hasStyleSheetWatcherSupport) {
          this.sheetActor = this.pageStyle._sheetRef(this._parentSheet);
          this.sheetActor.on("style-applied", this._onStyleApplied);
        }
      }
    } else {
      // Fake a rule
      this.type = ELEMENT_STYLE;
      this.rawNode = item;
      this.rawRule = {
        style: item.style,
        toString() {
          return "[element rule " + this.style + "]";
        },
      };
    }
  },

  get conn() {
    return this.pageStyle.conn;
  },

  destroy() {
    if (!this.rawStyle) {
      return;
    }
    protocol.Actor.prototype.destroy.call(this);
    this.rawStyle = null;
    this.pageStyle = null;
    this.rawNode = null;
    this.rawRule = null;
    this._declarations = null;
    if (this.sheetActor) {
      this.sheetActor.off("style-applied", this._onStyleApplied);
    }
  },

  // Objects returned by this actor are owned by the PageStyleActor
  // to which this rule belongs.
  get marshallPool() {
    return this.pageStyle;
  },

  // True if this rule supports as-authored styles, meaning that the
  // rule text can be rewritten using setRuleText.
  get canSetRuleText() {
    return (
      this.type === ELEMENT_STYLE ||
      (this._parentSheet &&
        // If a rule has been modified via CSSOM, then we should fall
        // back to non-authored editing.
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1224121
        !InspectorUtils.hasRulesModifiedByCSSOM(this._parentSheet) &&
        // Special case about:PreferenceStyleSheet, as it is generated on
        // the fly and the URI is not registered with the about:handler
        // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
        this._parentSheet.href !== "about:PreferenceStyleSheet")
    );
  },

  /**
   * Return an array with StyleRuleActor instances for each of this rule's ancestor rules
   * (@media, @supports, @keyframes, etc) obtained by recursively reading rule.parentRule.
   * If the rule has no ancestors, return an empty array.
   *
   * @return {Array}
   */
  get ancestorRules() {
    const ancestors = [];
    let rule = this.rawRule;

    while (rule.parentRule) {
      ancestors.unshift(this.pageStyle._styleRef(rule.parentRule));
      rule = rule.parentRule;
    }

    return ancestors;
  },

  /**
   * Return an object with information about this rule used for tracking changes.
   * It will be decorated with information about a CSS change before being tracked.
   *
   * It contains:
   * - the rule selector (or generated selectror for inline styles)
   * - the rule's host stylesheet (or element for inline styles)
   * - the rule's ancestor rules (@media, @supports, @keyframes), if any
   * - the rule's position within its ancestor tree, if any
   *
   * @return {Object}
   */
  get metadata() {
    const data = {};
    data.id = this.actorID;
    // Collect information about the rule's ancestors (@media, @supports, @keyframes).
    // Used to show context for this change in the UI and to match the rule for undo/redo.
    data.ancestors = this.ancestorRules.map(rule => {
      return {
        id: rule.actorID,
        // Rule type as number defined by CSSRule.type (ex: 4, 7, 12)
        // @see https://developer.mozilla.org/en-US/docs/Web/API/CSSRule
        type: rule.rawRule.type,
        // Rule type as human-readable string (ex: "@media", "@supports", "@keyframes")
        typeName: CSSRuleTypeName[rule.rawRule.type],
        // Conditions of @media and @supports rules (ex: "min-width: 1em")
        conditionText: rule.rawRule.conditionText,
        // Name of @keyframes rule; refrenced by the animation-name CSS property.
        name: rule.rawRule.name,
        // Selector of individual @keyframe rule within a @keyframes rule (ex: 0%, 100%).
        keyText: rule.rawRule.keyText,
        // Array with the indexes of this rule and its ancestors within the CSS rule tree.
        ruleIndex: rule._ruleIndex,
      };
    });

    // For changes in element style attributes, generate a unique selector.
    if (this.type === ELEMENT_STYLE && this.rawNode) {
      // findCssSelector() fails on XUL documents. Catch and silently ignore that error.
      try {
        data.selector = findCssSelector(this.rawNode);
      } catch (err) {}

      data.source = {
        type: "element",
        // Used to differentiate between elements which match the same generated selector
        // but live in different documents (ex: host document and iframe).
        href: this.rawNode.baseURI,
        // Element style attributes don't have a rule index; use the generated selector.
        index: data.selector,
        // Whether the element lives in a different frame than the host document.
        isFramed: this.rawNode.ownerGlobal !== this.pageStyle.ownerWindow,
      };

      const nodeActor = this.pageStyle.walker.getNode(this.rawNode);
      if (nodeActor) {
        data.source.id = nodeActor.actorID;
      }

      data.ruleIndex = 0;
    } else {
      data.selector =
        this.type === CSSRule.KEYFRAME_RULE
          ? this.rawRule.keyText
          : this.rawRule.selectorText;
      // Used to differentiate between changes to rules with identical selectors.
      data.ruleIndex = this._ruleIndex;

      if (this.pageStyle.hasStyleSheetWatcherSupport) {
        const sheet = this._parentSheet;
        const inspectorActor = this.pageStyle.inspector;
        const resourceId = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
          sheet
        );
        const styleSheetIndex = this.pageStyle.styleSheetsManager.getStyleSheetIndex(
          resourceId
        );
        data.source = {
          // Inline stylesheets have a null href; Use window URL instead.
          type: sheet.href ? "stylesheet" : "inline",
          href: sheet.href || inspectorActor.window.location.toString(),
          id: resourceId,
          index: styleSheetIndex,
          // Whether the stylesheet lives in a different frame than the host document.
          isFramed: inspectorActor.window !== inspectorActor.window.top,
        };
      } else {
        data.source = {
          // Inline stylesheets have a null href; Use window URL instead.
          type: this.sheetActor.href ? "stylesheet" : "inline",
          href:
            this.sheetActor.href || this.sheetActor.window.location.toString(),
          id: this.sheetActor.actorID,
          index: this.sheetActor.styleSheetIndex,
          // Whether the stylesheet lives in a different frame than the host document.
          isFramed: this.sheetActor.ownerWindow !== this.sheetActor.window,
        };
      }
    }

    return data;
  },

  getDocument(sheet) {
    if (!sheet.associatedDocument) {
      throw new Error(
        "Failed trying to get the document of an invalid stylesheet"
      );
    }
    return sheet.associatedDocument;
  },

  toString() {
    return "[StyleRuleActor for " + this.rawRule + "]";
  },

  // eslint-disable-next-line complexity
  form() {
    const form = {
      actor: this.actorID,
      type: this.type,
      line: this.line || undefined,
      column: this.column,
      ancestorData: [],
      traits: {
        // Indicates whether StyleRuleActor implements and can use the setRuleText method.
        // It cannot use it if the stylesheet was programmatically mutated via the CSSOM.
        canSetRuleText: this.canSetRuleText,
      },
    };

    // Go through all ancestor so we can build an array of all the media queries and
    // layers this rule is in.
    for (const ancestorRule of this.ancestorRules) {
      const ruleClassName = ChromeUtils.getClassName(ancestorRule.rawRule);
      if (
        ruleClassName === "CSSMediaRule" &&
        ancestorRule.rawRule.media?.length
      ) {
        form.ancestorData.push({
          type: "media",
          value: Array.from(ancestorRule.rawRule.media).join(", "),
        });
      } else if (ruleClassName === "CSSLayerBlockRule") {
        form.ancestorData.push({
          type: "layer",
          value: ancestorRule.rawRule.name,
        });
      } else if (ruleClassName === "CSSContainerRule") {
        form.ancestorData.push({
          type: "container",
          // Send containerName and containerQuery separately (instead of conditionText)
          // so the client has more flexibility to display the information.
          containerName: ancestorRule.rawRule.containerName,
          containerQuery: ancestorRule.rawRule.containerQuery,
        });
      } else if (ruleClassName === "CSSSupportsRule") {
        form.ancestorData.push({
          type: "supports",
          conditionText: ancestorRule.rawRule.conditionText,
        });
      }
    }

    if (this._parentSheet) {
      if (this.pageStyle.hasStyleSheetWatcherSupport) {
        form.parentStyleSheet = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
          this._parentSheet
        );
      } else {
        form.parentStyleSheet = this.pageStyle._sheetRef(
          this._parentSheet
        ).actorID;
      }

      // If the rule is in a imported stylesheet with a specified layer, put it at the top
      // of the ancestor data array.
      if (
        this._parentSheet.ownerRule &&
        this._parentSheet.ownerRule.layerName !== null
      ) {
        form.ancestorData.unshift({
          type: "layer",
          value: this._parentSheet.ownerRule.layerName,
        });
      }
    }

    // One tricky thing here is that other methods in this actor must
    // ensure that authoredText has been set before |form| is called.
    // This has to be treated specially, for now, because we cannot
    // synchronously compute the authored text, but |form| also cannot
    // return a promise.  See bug 1205868.
    form.authoredText = this.authoredText;

    switch (this.type) {
      case CSSRule.STYLE_RULE:
        form.selectors = CssLogic.getSelectors(this.rawRule);
        form.cssText = this.rawStyle.cssText || "";
        break;
      case ELEMENT_STYLE:
        // Elements don't have a parent stylesheet, and therefore
        // don't have an associated URI.  Provide a URI for
        // those.
        const doc = this.rawNode.ownerDocument;
        form.href = doc.location ? doc.location.href : "";
        form.cssText = this.rawStyle.cssText || "";
        form.authoredText = this.rawNode.getAttribute("style");
        break;
      case CSSRule.CHARSET_RULE:
        form.encoding = this.rawRule.encoding;
        break;
      case CSSRule.IMPORT_RULE:
        form.href = this.rawRule.href;
        break;
      case CSSRule.KEYFRAMES_RULE:
        form.cssText = this.rawRule.cssText;
        form.name = this.rawRule.name;
        break;
      case CSSRule.KEYFRAME_RULE:
        form.cssText = this.rawStyle.cssText || "";
        form.keyText = this.rawRule.keyText || "";
        break;
    }

    // Parse the text into a list of declarations so the client doesn't have to
    // and so that we can safely determine if a declaration is valid rather than
    // have the client guess it.
    if (form.authoredText || form.cssText) {
      // authoredText may be an empty string when deleting all properties; it's ok to use.
      const cssText =
        typeof form.authoredText === "string"
          ? form.authoredText
          : form.cssText;
      const declarations = parseNamedDeclarations(
        isCssPropertyKnown,
        cssText,
        true
      );
      const el = this.pageStyle.selectedElement;
      const style = this.pageStyle.cssLogic.computedStyle;

      // Whether the stylesheet is a user-agent stylesheet. This affects the
      // validity of some properties and property values.
      const userAgent =
        this._parentSheet &&
        SharedCssLogic.isAgentStylesheet(this._parentSheet);
      // Whether the stylesheet is a chrome stylesheet. Ditto.
      //
      // Note that chrome rules are also enabled in user sheets, see
      // ParserContext::chrome_rules_enabled().
      //
      // https://searchfox.org/mozilla-central/rev/919607a3610222099fbfb0113c98b77888ebcbfb/servo/components/style/parser.rs#164
      const chrome = (() => {
        if (!this._parentSheet) {
          return false;
        }
        if (SharedCssLogic.isUserStylesheet(this._parentSheet)) {
          return true;
        }
        if (this._parentSheet.href) {
          return this._parentSheet.href.startsWith("chrome:");
        }
        return el && el.ownerDocument.documentURI.startsWith("chrome:");
      })();
      // Whether the document is in quirks mode. This affects whether stuff
      // like `width: 10` is valid.
      const quirks =
        !userAgent && el && el.ownerDocument.compatMode == "BackCompat";
      const supportsOptions = { userAgent, chrome, quirks };
      form.declarations = declarations.map(decl => {
        // InspectorUtils.supports only supports the 1-arg version, but that's
        // what we want to do anyways so that we also accept !important in the
        // value.
        decl.isValid = InspectorUtils.supports(
          `${decl.name}:${decl.value}`,
          supportsOptions
        );
        // TODO: convert from Object to Boolean. See Bug 1574471
        decl.isUsed = isPropertyUsed(el, style, this.rawRule, decl.name);
        // Check property name. All valid CSS properties support "initial" as a value.
        decl.isNameValid = InspectorUtils.supports(
          `${decl.name}:initial`,
          supportsOptions
        );
        return decl;
      });

      // We have computed the new `declarations` array, before forgetting about
      // the old declarations compute the CSS changes for pending modifications
      // applied by the user. Comparing the old and new declarations arrays
      // ensures we only rely on values understood by the engine and not authored
      // values. See Bug 1590031.
      this._pendingDeclarationChanges.forEach(change =>
        this.logDeclarationChange(change, declarations, this._declarations)
      );
      this._pendingDeclarationChanges = [];

      // Cache parsed declarations so we don't needlessly re-parse authoredText every time
      // we need to check previous property names and values when tracking changes.
      this._declarations = declarations;
    }

    return form;
  },

  /**
   * Send an event notifying that the location of the rule has
   * changed.
   *
   * @param {Number} line the new line number
   * @param {Number} column the new column number
   */
  _notifyLocationChanged(line, column) {
    this.emit("location-changed", line, column);
  },

  /**
   * Compute the index of this actor's raw rule in its parent style
   * sheet.  The index is a vector where each element is the index of
   * a given CSS rule in its parent.  A vector is used to support
   * nested rules.
   */
  _computeRuleIndex() {
    let rule = this.rawRule;
    const result = [];

    while (rule) {
      let cssRules = [];
      if (rule.parentRule) {
        cssRules = rule.parentRule.cssRules;
      } else if (rule.parentStyleSheet) {
        cssRules = rule.parentStyleSheet.cssRules;
      }

      let found = false;
      for (let i = 0; i < cssRules.length; i++) {
        if (rule === cssRules.item(i)) {
          found = true;
          result.unshift(i);
          break;
        }
      }

      if (!found) {
        this._ruleIndex = null;
        return;
      }

      rule = rule.parentRule;
    }

    this._ruleIndex = result;
  },

  /**
   * Get the rule corresponding to |this._ruleIndex| from the given
   * style sheet.
   *
   * @param  {DOMStyleSheet} sheet
   *         The style sheet.
   * @return {CSSStyleRule} the rule corresponding to
   * |this._ruleIndex|
   */
  _getRuleFromIndex(parentSheet) {
    let currentRule = null;
    for (const i of this._ruleIndex) {
      if (currentRule === null) {
        currentRule = parentSheet.cssRules[i];
      } else {
        currentRule = currentRule.cssRules.item(i);
      }
    }
    return currentRule;
  },

  /**
   * This is attached to the parent style sheet actor's
   * "style-applied" event.
   */
  _onStyleApplied(kind) {
    if (kind === UPDATE_GENERAL) {
      // A general change means that the rule actors are invalidated,
      // so stop listening to events now.
      if (this.sheetActor) {
        this.sheetActor.off("style-applied", this._onStyleApplied);
      }
    } else if (this._ruleIndex) {
      // The sheet was updated by this actor, in a way that preserves
      // the rules.  Now, recompute our new rule from the style sheet,
      // so that we aren't left with a reference to a dangling rule.
      const oldRule = this.rawRule;
      const oldActor = this.pageStyle.refMap.get(oldRule);
      this.rawRule = this._getRuleFromIndex(this._parentSheet);
      if (oldActor) {
        // Also tell the page style so that future calls to _styleRef
        // return the same StyleRuleActor.
        this.pageStyle.updateStyleRef(oldRule, this.rawRule, this);
      }
      const line = InspectorUtils.getRelativeRuleLine(this.rawRule);
      const column = InspectorUtils.getRuleColumn(this.rawRule);
      if (line !== this.line || column !== this.column) {
        this._notifyLocationChanged(line, column);
      }
      this.line = line;
      this.column = column;
    }
  },

  /**
   * Return a promise that resolves to the authored form of a rule's
   * text, if available.  If the authored form is not available, the
   * returned promise simply resolves to the empty string.  If the
   * authored form is available, this also sets |this.authoredText|.
   * The authored text will include invalid and otherwise ignored
   * properties.
   *
   * @param {Boolean} skipCache
   *        If a value for authoredText was previously found and cached,
   *        ignore it and parse the stylehseet again. The authoredText
   *        may be outdated if a descendant of this rule has changed.
   */
  async getAuthoredCssText(skipCache = false) {
    if (!this.canSetRuleText || !SUPPORTED_RULE_TYPES.includes(this.type)) {
      return Promise.resolve("");
    }

    if (typeof this.authoredText === "string" && !skipCache) {
      return Promise.resolve(this.authoredText);
    }

    if (this.pageStyle.hasStyleSheetWatcherSupport) {
      const resourceId = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
        this._parentSheet
      );
      const cssText = await this.pageStyle.styleSheetsManager.getText(
        resourceId
      );
      const { text } = getRuleText(cssText, this.line, this.column);

      // Cache the result on the rule actor to avoid parsing again next time
      this.authoredText = text;
      return this.authoredText;
    }

    return this.sheetActor.getText().then(longStr => {
      const cssText = longStr.str;
      const { text } = getRuleText(cssText, this.line, this.column);

      // Cache the result on the rule actor to avoid parsing again next time
      this.authoredText = text;
      return this.authoredText;
    });
  },

  /**
   * Return a promise that resolves to the complete cssText of the rule as authored.
   *
   * Unlike |getAuthoredCssText()|, which only returns the contents of the rule, this
   * method includes the CSS selectors and at-rules (@media, @supports, @keyframes, etc.)
   *
   * If the rule type is unrecongized, the promise resolves to an empty string.
   * If the rule is an element inline style, the promise resolves with the generated
   * selector that uniquely identifies the element and with the rule body consisting of
   * the element's style attribute.
   *
   * @return {String}
   */
  async getRuleText() {
    // Bail out if the rule is not supported or not an element inline style.
    if (![...SUPPORTED_RULE_TYPES, ELEMENT_STYLE].includes(this.type)) {
      return Promise.resolve("");
    }

    let ruleBodyText;
    let selectorText;
    let text;

    // For element inline styles, use the style attribute and generated unique selector.
    if (this.type === ELEMENT_STYLE) {
      ruleBodyText = this.rawNode.getAttribute("style");
      selectorText = this.metadata.selector;
    } else {
      // Get the rule's authored text and skip any cached value.
      ruleBodyText = await this.getAuthoredCssText(true);

      let stylesheetText = null;
      if (this.pageStyle.hasStyleSheetWatcherSupport) {
        const resourceId = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
          this._parentSheet
        );
        stylesheetText = await this.pageStyle.styleSheetsManager.getText(
          resourceId
        );
      } else {
        const { str } = await this.sheetActor.getText();
        stylesheetText = str;
      }

      const [start, end] = getSelectorOffsets(
        stylesheetText,
        this.line,
        this.column
      );
      selectorText = stylesheetText.substring(start, end);
    }

    // CSS rule type as a string "@media", "@supports", "@keyframes", etc.
    const typeName = CSSRuleTypeName[this.type];

    // When dealing with at-rules, getSelectorOffsets() will not return the rule type.
    // We prepend it ourselves.
    if (typeName) {
      text = `${typeName}${selectorText} {${ruleBodyText}}`;
    } else {
      text = `${selectorText} {${ruleBodyText}}`;
    }

    const { result } = prettifyCSS(text);
    return Promise.resolve(result);
  },

  /**
   * Set the contents of the rule.  This rewrites the rule in the
   * stylesheet and causes it to be re-evaluated.
   *
   * @param {String} newText
   *        The new text of the rule
   * @param {Array} modifications
   *        Array with modifications applied to the rule. Contains objects like:
   *        {
   *          type: "set",
   *          index: <number>,
   *          name: <string>,
   *          value: <string>,
   *          priority: <optional string>
   *        }
   *         or
   *        {
   *          type: "remove",
   *          index: <number>,
   *          name: <string>,
   *        }
   * @returns the rule with updated properties
   */
  async setRuleText(newText, modifications = []) {
    if (!this.canSetRuleText) {
      throw new Error("invalid call to setRuleText");
    }

    if (this.type === ELEMENT_STYLE) {
      // For element style rules, set the node's style attribute.
      this.rawNode.setAttributeDevtools("style", newText);
    } else if (this.pageStyle.hasStyleSheetWatcherSupport) {
      const resourceId = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
        this._parentSheet
      );
      let cssText = await this.pageStyle.styleSheetsManager.getText(resourceId);

      const { offset, text } = getRuleText(cssText, this.line, this.column);
      cssText =
        cssText.substring(0, offset) +
        newText +
        cssText.substring(offset + text.length);

      await this.pageStyle.styleSheetsManager.setStyleSheetText(
        resourceId,
        cssText,
        { kind: UPDATE_PRESERVING_RULES }
      );
    } else {
      // For stylesheet rules, set the text in the stylesheet.
      const parentStyleSheet = this.pageStyle._sheetRef(this._parentSheet);
      let { str: cssText } = await parentStyleSheet.getText();

      const { offset, text } = getRuleText(cssText, this.line, this.column);
      cssText =
        cssText.substring(0, offset) +
        newText +
        cssText.substring(offset + text.length);

      await parentStyleSheet.update(cssText, false, UPDATE_PRESERVING_RULES);
    }

    this.authoredText = newText;
    this.pageStyle.refreshObservedRules();

    // Add processed modifications to the _pendingDeclarationChanges array,
    // they will be emitted as CSS_CHANGE resources once `declarations` have
    // been re-computed in `form`.
    this._pendingDeclarationChanges.push(...modifications);

    // Returning this updated actor over the protocol will update its corresponding front
    // and any references to it.
    return this;
  },

  /**
   * Modify a rule's properties. Passed an array of modifications:
   * {
   *   type: "set",
   *   index: <number>,
   *   name: <string>,
   *   value: <string>,
   *   priority: <optional string>
   * }
   *  or
   * {
   *   type: "remove",
   *   index: <number>,
   *   name: <string>,
   * }
   *
   * @returns the rule with updated properties
   */
  modifyProperties(modifications) {
    // Use a fresh element for each call to this function to prevent side
    // effects that pop up based on property values that were already set on the
    // element.

    let document;
    if (this.rawNode) {
      document = this.rawNode.ownerDocument;
    } else {
      let parentStyleSheet = this._parentSheet;
      while (parentStyleSheet.ownerRule) {
        parentStyleSheet = parentStyleSheet.ownerRule.parentStyleSheet;
      }

      document = this.getDocument(parentStyleSheet);
    }

    const tempElement = document.createElementNS(XHTML_NS, "div");

    for (const mod of modifications) {
      if (mod.type === "set") {
        tempElement.style.setProperty(mod.name, mod.value, mod.priority || "");
        this.rawStyle.setProperty(
          mod.name,
          tempElement.style.getPropertyValue(mod.name),
          mod.priority || ""
        );
      } else if (mod.type === "remove" || mod.type === "disable") {
        this.rawStyle.removeProperty(mod.name);
      }
    }

    this.pageStyle.refreshObservedRules();

    // Add processed modifications to the _pendingDeclarationChanges array,
    // they will be emitted as CSS_CHANGE resources once `declarations` have
    // been re-computed in `form`.
    this._pendingDeclarationChanges.push(...modifications);

    return this;
  },

  /**
   * Helper function for modifySelector, inserts the new
   * rule with the new selector into the parent style sheet and removes the
   * current rule. Returns the newly inserted css rule or null if the rule is
   * unsuccessfully inserted to the parent style sheet.
   *
   * @param {String} value
   *        The new selector value
   * @param {Boolean} editAuthored
   *        True if the selector should be updated by editing the
   *        authored text; false if the selector should be updated via
   *        CSSOM.
   *
   * @returns {CSSRule}
   *        The new CSS rule added
   */
  async _addNewSelector(value, editAuthored) {
    const rule = this.rawRule;
    const parentStyleSheet = this._parentSheet;

    // We know the selector modification is ok, so if the client asked
    // for the authored text to be edited, do it now.
    if (editAuthored) {
      const document = this.getDocument(this._parentSheet);
      try {
        document.querySelector(value);
      } catch (e) {
        return null;
      }

      if (this.pageStyle.hasStyleSheetWatcherSupport) {
        const resourceId = this.pageStyle.styleSheetsManager.getStyleSheetResourceId(
          this._parentSheet
        );
        let authoredText = await this.pageStyle.styleSheetsManager.getText(
          resourceId
        );

        const [startOffset, endOffset] = getSelectorOffsets(
          authoredText,
          this.line,
          this.column
        );
        authoredText =
          authoredText.substring(0, startOffset) +
          value +
          authoredText.substring(endOffset);

        await this.pageStyle.styleSheetsManager.setStyleSheetText(
          resourceId,
          authoredText,
          { kind: UPDATE_PRESERVING_RULES }
        );
      } else {
        const sheetActor = this.pageStyle._sheetRef(parentStyleSheet);
        let { str: authoredText } = await sheetActor.getText();

        const [startOffset, endOffset] = getSelectorOffsets(
          authoredText,
          this.line,
          this.column
        );
        authoredText =
          authoredText.substring(0, startOffset) +
          value +
          authoredText.substring(endOffset);

        await sheetActor.update(authoredText, false, UPDATE_PRESERVING_RULES);
      }
    } else {
      const cssRules = parentStyleSheet.cssRules;
      const cssText = rule.cssText;
      const selectorText = rule.selectorText;

      for (let i = 0; i < cssRules.length; i++) {
        if (rule === cssRules.item(i)) {
          try {
            // Inserts the new style rule into the current style sheet and
            // delete the current rule
            const ruleText = cssText.slice(selectorText.length).trim();
            parentStyleSheet.insertRule(value + " " + ruleText, i);
            parentStyleSheet.deleteRule(i + 1);
            break;
          } catch (e) {
            // The selector could be invalid, or the rule could fail to insert.
            return null;
          }
        }
      }
    }

    return this._getRuleFromIndex(parentStyleSheet);
  },

  /**
   * Take an object with instructions to modify a CSS declaration and log an object with
   * normalized metadata which describes the change in the context of this rule.
   *
   * @param {Object} change
   *        Data about a modification to a declaration. @see |modifyProperties()|
   * @param {Object} newDeclarations
   *        The current declarations array to get the latest values, names...
   * @param {Object} oldDeclarations
   *        The previous declarations array to use to fetch old values, names...
   */
  logDeclarationChange(change, newDeclarations, oldDeclarations) {
    // Position of the declaration within its rule.
    const index = change.index;
    // Destructure properties from the previous CSS declaration at this index, if any,
    // to new variable names to indicate the previous state.
    let {
      value: prevValue,
      name: prevName,
      priority: prevPriority,
      commentOffsets,
    } = oldDeclarations[index] || {};

    const { value: currentValue, name: currentName } =
      newDeclarations[index] || {};
    // A declaration is disabled if it has a `commentOffsets` array.
    // Here we type coerce the value to a boolean with double-bang (!!)
    const prevDisabled = !!commentOffsets;
    // Append the "!important" string if defined in the previous priority flag.
    prevValue =
      prevValue && prevPriority ? `${prevValue} !important` : prevValue;

    const data = this.metadata;

    switch (change.type) {
      case "set":
        data.type = prevValue ? "declaration-add" : "declaration-update";
        // If `change.newName` is defined, use it because the property is being renamed.
        // Otherwise, a new declaration is being created or the value of an existing
        // declaration is being updated. In that case, use the currentName computed
        // by the engine.
        const changeName = currentName || change.name;
        const name = change.newName ? change.newName : changeName;
        // Append the "!important" string if defined in the incoming priority flag.

        const changeValue = currentValue || change.value;
        const newValue = change.priority
          ? `${changeValue} !important`
          : changeValue;

        // Reuse the previous value string, when the property is renamed.
        // Otherwise, use the incoming value string.
        const value = change.newName ? prevValue : newValue;

        data.add = [{ property: name, value, index }];
        // If there is a previous value, log its removal together with the previous
        // property name. Using the previous name handles the case for renaming a property
        // and is harmless when updating an existing value (the name stays the same).
        if (prevValue) {
          data.remove = [{ property: prevName, value: prevValue, index }];
        } else {
          data.remove = null;
        }

        // When toggling a declaration from OFF to ON, if not renaming the property,
        // do not mark the previous declaration for removal, otherwise the add and
        // remove operations will cancel each other out when tracked. Tracked changes
        // have no context of "disabled", only "add" or remove, like diffs.
        if (prevDisabled && !change.newName && prevValue === newValue) {
          data.remove = null;
        }

        break;

      case "remove":
        data.type = "declaration-remove";
        data.add = null;
        data.remove = [{ property: change.name, value: prevValue, index }];
        break;

      case "disable":
        data.type = "declaration-disable";
        data.add = null;
        data.remove = [{ property: change.name, value: prevValue, index }];
        break;
    }

    TrackChangeEmitter.trackChange(data);
  },

  /**
   * Helper method for tracking CSS changes. Logs the change of this rule's selector as
   * two operations: a removal using the old selector and an addition using the new one.
   *
   * @param {String} oldSelector
   *        This rule's previous selector.
   * @param {String} newSelector
   *        This rule's new selector.
   */
  logSelectorChange(oldSelector, newSelector) {
    TrackChangeEmitter.trackChange({
      ...this.metadata,
      type: "selector-remove",
      add: null,
      remove: null,
      selector: oldSelector,
    });

    TrackChangeEmitter.trackChange({
      ...this.metadata,
      type: "selector-add",
      add: null,
      remove: null,
      selector: newSelector,
    });
  },

  /**
   * Modify the current rule's selector by inserting a new rule with the new
   * selector value and removing the current rule.
   *
   * Returns information about the new rule and applied style
   * so that consumers can immediately display the new rule, whether or not the
   * selector matches the current element without having to refresh the whole
   * list.
   *
   * @param {DOMNode} node
   *        The current selected element
   * @param {String} value
   *        The new selector value
   * @param {Boolean} editAuthored
   *        True if the selector should be updated by editing the
   *        authored text; false if the selector should be updated via
   *        CSSOM.
   * @returns {Object}
   *        Returns an object that contains the applied style properties of the
   *        new rule and a boolean indicating whether or not the new selector
   *        matches the current selected element
   */
  modifySelector(node, value, editAuthored = false) {
    if (this.type === ELEMENT_STYLE || this.rawRule.selectorText === value) {
      return { ruleProps: null, isMatching: true };
    }

    // The rule's previous selector is lost after calling _addNewSelector(). Save it now.
    const oldValue = this.rawRule.selectorText;
    let selectorPromise = this._addNewSelector(value, editAuthored);

    if (editAuthored) {
      selectorPromise = selectorPromise.then(newCssRule => {
        if (newCssRule) {
          this.logSelectorChange(oldValue, value);
          const style = this.pageStyle._styleRef(newCssRule);
          // See the comment in |form| to understand this.
          return style.getAuthoredCssText().then(() => newCssRule);
        }
        return newCssRule;
      });
    }

    return selectorPromise.then(newCssRule => {
      let ruleProps = null;
      let isMatching = false;

      if (newCssRule) {
        const ruleEntry = this.pageStyle.findEntryMatchingRule(
          node,
          newCssRule
        );
        if (ruleEntry.length === 1) {
          ruleProps = this.pageStyle.getAppliedProps(node, ruleEntry, {
            matchedSelectors: true,
          });
        } else {
          ruleProps = this.pageStyle.getNewAppliedProps(node, newCssRule);
        }

        isMatching = ruleProps.entries.some(
          ruleProp => !!ruleProp.matchedSelectors.length
        );
      }

      return { ruleProps, isMatching };
    });
  },

  /**
   * Using the latest computed style applicable to the selected element,
   * check the states of declarations in this CSS rule.
   *
   * If any have changed their used/unused state, potentially as a result of changes in
   * another rule, fire a "rule-updated" event with this rule actor in its latest state.
   */
  refresh() {
    let hasChanged = false;
    const el = this.pageStyle.selectedElement;
    const style = CssLogic.getComputedStyle(el);

    for (const decl of this._declarations) {
      // TODO: convert from Object to Boolean. See Bug 1574471
      const isUsed = isPropertyUsed(el, style, this.rawRule, decl.name);

      if (decl.isUsed.used !== isUsed.used) {
        decl.isUsed = isUsed;
        hasChanged = true;
      }
    }

    if (hasChanged) {
      // ⚠️ IMPORTANT ⚠️
      // When an event is emitted via the protocol with the StyleRuleActor as payload, the
      // corresponding StyleRuleFront will be automatically updated under the hood.
      // Therefore, when the client looks up properties on the front reference it already
      // has, it will get the latest values set on the actor, not the ones it originally
      // had when the front was created. The client is not required to explicitly replace
      // its previous front reference to the one it receives as this event's payload.
      // The client doesn't even need to explicitly listen for this event.
      // The update of the front happens automatically.
      this.emit("rule-updated", this);
    }
  },
});
exports.StyleRuleActor = StyleRuleActor;

/**
 * Compute the start and end offsets of a rule's selector text, given
 * the CSS text and the line and column at which the rule begins.
 * @param {String} initialText
 * @param {Number} line (1-indexed)
 * @param {Number} column (1-indexed)
 * @return {array} An array with two elements: [startOffset, endOffset].
 *                 The elements mark the bounds in |initialText| of
 *                 the CSS rule's selector.
 */
function getSelectorOffsets(initialText, line, column) {
  if (typeof line === "undefined" || typeof column === "undefined") {
    throw new Error("Location information is missing");
  }

  const { offset: textOffset, text } = getTextAtLineColumn(
    initialText,
    line,
    column
  );
  const lexer = getCSSLexer(text);

  // Search forward for the opening brace.
  let endOffset;
  while (true) {
    const token = lexer.nextToken();
    if (!token) {
      break;
    }
    if (token.tokenType === "symbol" && token.text === "{") {
      if (endOffset === undefined) {
        break;
      }
      return [textOffset, textOffset + endOffset];
    }
    // Preserve comments and whitespace just before the "{".
    if (token.tokenType !== "comment" && token.tokenType !== "whitespace") {
      endOffset = token.endOffset;
    }
  }

  throw new Error("could not find bounds of rule");
}
