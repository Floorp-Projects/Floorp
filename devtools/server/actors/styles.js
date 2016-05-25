/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const promise = require("promise");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const {getDefinedGeometryProperties} = require("devtools/server/actors/highlighters/geometry-editor");
const {parseDeclarations} = require("devtools/shared/css-parsing-utils");
const {isCssPropertyKnown} = require("devtools/server/actors/css-properties");
const {Task} = require("devtools/shared/task");
const events = require("sdk/event/core");

// This will also add the "stylesheet" actor type for protocol.js to recognize
const {UPDATE_PRESERVING_RULES, UPDATE_GENERAL} = require("devtools/server/actors/stylesheets");
const {pageStyleSpec, styleRuleSpec} = require("devtools/shared/specs/styles");

loader.lazyRequireGetter(this, "CSS", "CSS");
loader.lazyGetter(this, "CssLogic", () => require("devtools/shared/inspector/css-logic").CssLogic);
loader.lazyGetter(this, "DOMUtils", () => Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils));

// The PageStyle actor flattens the DOM CSS objects a little bit, merging
// Rules and their Styles into one actor.  For elements (which have a style
// but no associated rule) we fake a rule with the following style id.
const ELEMENT_STYLE = 100;
exports.ELEMENT_STYLE = ELEMENT_STYLE;

// When gathering rules to read for pseudo elements, we will skip
// :before and :after, which are handled as a special case.
loader.lazyGetter(this, "PSEUDO_ELEMENTS_TO_READ", () => {
  return DOMUtils.getCSSPseudoElementNames().filter(pseudo => {
    return pseudo !== ":before" && pseudo !== ":after";
  });
});

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const FONT_PREVIEW_TEXT = "Abc";
const FONT_PREVIEW_FONT_SIZE = 40;
const FONT_PREVIEW_FILLSTYLE = "black";
const NORMAL_FONT_WEIGHT = 400;
const BOLD_FONT_WEIGHT = 700;
// Offset (in px) to avoid cutting off text edges of italic fonts.
const FONT_PREVIEW_OFFSET = 4;

/**
 * The PageStyle actor lets the client look at the styles on a page, as
 * they are applied to a given node.
 */
var PageStyleActor = protocol.ActorClassWithSpec(pageStyleSpec, {
  /**
   * Create a PageStyleActor.
   *
   * @param inspector
   *    The InspectorActor that owns this PageStyleActor.
   *
   * @constructor
   */
  initialize: function (inspector) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.inspector = inspector;
    if (!this.inspector.walker) {
      throw Error("The inspector's WalkerActor must be created before " +
                   "creating a PageStyleActor.");
    }
    this.walker = inspector.walker;
    this.cssLogic = new CssLogic();

    // Stores the association of DOM objects -> actors
    this.refMap = new Map();

    // Maps document elements to style elements, used to add new rules.
    this.styleElements = new WeakMap();

    this.onFrameUnload = this.onFrameUnload.bind(this);
    events.on(this.inspector.tabActor, "will-navigate", this.onFrameUnload);

    this._styleApplied = this._styleApplied.bind(this);
    this._watchedSheets = new Set();
  },

  destroy: function () {
    if (!this.walker) {
      return;
    }
    protocol.Actor.prototype.destroy.call(this);
    events.off(this.inspector.tabActor, "will-navigate", this.onFrameUnload);
    this.inspector = null;
    this.walker = null;
    this.refMap = null;
    this.cssLogic = null;
    this.styleElements = null;

    for (let sheet of this._watchedSheets) {
      sheet.off("style-applied", this._styleApplied);
    }
    this._watchedSheets.clear();
  },

  get conn() {
    return this.inspector.conn;
  },

  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    return {
      actor: this.actorID,
      traits: {
        // Whether the actor has had bug 1103993 fixed, which means that the
        // getApplied method calls cssLogic.highlight(node) to recreate the
        // style cache. Clients requesting getApplied from actors that have not
        // been fixed must make sure cssLogic.highlight(node) was called before.
        getAppliedCreatesStyleCache: true,
        // Whether addNewRule accepts the editAuthored argument.
        authoredStyles: true
      }
    };
  },

  /**
   * Called when a style sheet is updated.
   */
  _styleApplied: function (kind, styleSheet) {
    // No matter what kind of update is done, we need to invalidate
    // the keyframe cache.
    this.cssLogic.reset();
    if (kind === UPDATE_GENERAL) {
      events.emit(this, "stylesheet-updated", styleSheet);
    }
  },

  /**
   * Return or create a StyleRuleActor for the given item.
   * @param item Either a CSSStyleRule or a DOM element.
   */
  _styleRef: function (item) {
    if (this.refMap.has(item)) {
      return this.refMap.get(item);
    }
    let actor = StyleRuleActor(this, item);
    this.manage(actor);
    this.refMap.set(item, actor);

    return actor;
  },

  /**
   * Update the association between a StyleRuleActor and its
   * corresponding item.  This is used when a StyleRuleActor updates
   * as style sheet and starts using a new rule.
   *
   * @param oldItem The old association; either a CSSStyleRule or a
   *                DOM element.
   * @param item Either a CSSStyleRule or a DOM element.
   * @param actor a StyleRuleActor
   */
  updateStyleRef: function (oldItem, item, actor) {
    this.refMap.delete(oldItem);
    this.refMap.set(item, actor);
  },

  /**
   * Return or create a StyleSheetActor for the given nsIDOMCSSStyleSheet.
   * @param  {DOMStyleSheet} sheet
   *         The style sheet to create an actor for.
   * @return {StyleSheetActor}
   *         The actor for this style sheet
   */
  _sheetRef: function (sheet) {
    let tabActor = this.inspector.tabActor;
    let actor = tabActor.createStyleSheetActor(sheet);
    if (!this._watchedSheets.has(actor)) {
      this._watchedSheets.add(actor);
      actor.on("style-applied", this._styleApplied);
    }
    return actor;
  },

  /**
   * Get the computed style for a node.
   *
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `markMatched`: true if you want the 'matched' property to be added
   *     when a computed property has been modified by a style included
   *     by `filter`.
   *   `onlyMatched`: true if unmatched properties shouldn't be included.
   *
   * @returns a JSON blob with the following form:
   *   {
   *     "property-name": {
   *       value: "property-value",
   *       priority: "!important" <optional>
   *       matched: <true if there are matched selectors for this value>
   *     },
   *     ...
   *   }
   */
  getComputed: function (node, options) {
    let ret = Object.create(null);

    this.cssLogic.sourceFilter = options.filter || CssLogic.FILTER.UA;
    this.cssLogic.highlight(node.rawNode);
    let computed = this.cssLogic.computedStyle || [];

    Array.prototype.forEach.call(computed, name => {
      ret[name] = {
        value: computed.getPropertyValue(name),
        priority: computed.getPropertyPriority(name) || undefined
      };
    });

    if (options.markMatched || options.onlyMatched) {
      let matched = this.cssLogic.hasMatchedSelectors(Object.keys(ret));
      for (let key in ret) {
        if (matched[key]) {
          ret[key].matched = options.markMatched ? true : undefined;
        } else if (options.onlyMatched) {
          delete ret[key];
        }
      }
    }

    return ret;
  },

  /**
   * Get all the fonts from a page.
   *
   * @param object options
   *   `includePreviews`: Whether to also return image previews of the fonts.
   *   `previewText`: The text to display in the previews.
   *   `previewFontSize`: The font size of the text in the previews.
   *
   * @returns object
   *   object with 'fontFaces', a list of fonts that apply to this node.
   */
  getAllUsedFontFaces: function (options) {
    let windows = this.inspector.tabActor.windows;
    let fontsList = [];
    for (let win of windows) {
      fontsList = [...fontsList,
                   ...this.getUsedFontFaces(win.document.body, options)];
    }
    return fontsList;
  },

  /**
   * Get the font faces used in an element.
   *
   * @param NodeActor node / actual DOM node
   *    The node to get fonts from.
   * @param object options
   *   `includePreviews`: Whether to also return image previews of the fonts.
   *   `previewText`: The text to display in the previews.
   *   `previewFontSize`: The font size of the text in the previews.
   *
   * @returns object
   *   object with 'fontFaces', a list of fonts that apply to this node.
   */
  getUsedFontFaces: function (node, options) {
    // node.rawNode is defined for NodeActor objects
    let actualNode = node.rawNode || node;
    let contentDocument = actualNode.ownerDocument;
    // We don't get fonts for a node, but for a range
    let rng = contentDocument.createRange();
    rng.selectNodeContents(actualNode);
    let fonts = DOMUtils.getUsedFontFaces(rng);
    let fontsArray = [];

    for (let i = 0; i < fonts.length; i++) {
      let font = fonts.item(i);
      let fontFace = {
        name: font.name,
        CSSFamilyName: font.CSSFamilyName,
        srcIndex: font.srcIndex,
        URI: font.URI,
        format: font.format,
        localName: font.localName,
        metadata: font.metadata
      };

      // If this font comes from a @font-face rule
      if (font.rule) {
        let styleActor = StyleRuleActor(this, font.rule);
        this.manage(styleActor);
        fontFace.rule = styleActor;
        fontFace.ruleText = font.rule.cssText;
      }

      // Get the weight and style of this font for the preview and sort order
      let weight = NORMAL_FONT_WEIGHT, style = "";
      if (font.rule) {
        weight = font.rule.style.getPropertyValue("font-weight")
                 || NORMAL_FONT_WEIGHT;
        if (weight == "bold") {
          weight = BOLD_FONT_WEIGHT;
        } else if (weight == "normal") {
          weight = NORMAL_FONT_WEIGHT;
        }
        style = font.rule.style.getPropertyValue("font-style") || "";
      }
      fontFace.weight = weight;
      fontFace.style = style;

      if (options.includePreviews) {
        let opts = {
          previewText: options.previewText,
          previewFontSize: options.previewFontSize,
          fontStyle: weight + " " + style,
          fillStyle: options.previewFillStyle
        };
        let { dataURL, size } = getFontPreviewData(font.CSSFamilyName,
                                                   contentDocument, opts);
        fontFace.preview = {
          data: LongStringActor(this.conn, dataURL),
          size: size
        };
      }
      fontsArray.push(fontFace);
    }

    // @font-face fonts at the top, then alphabetically, then by weight
    fontsArray.sort(function (a, b) {
      return a.weight > b.weight ? 1 : -1;
    });
    fontsArray.sort(function (a, b) {
      if (a.CSSFamilyName == b.CSSFamilyName) {
        return 0;
      }
      return a.CSSFamilyName > b.CSSFamilyName ? 1 : -1;
    });
    fontsArray.sort(function (a, b) {
      if ((a.rule && b.rule) || (!a.rule && !b.rule)) {
        return 0;
      }
      return !a.rule && b.rule ? 1 : -1;
    });

    return fontsArray;
  },

  /**
   * Get a list of selectors that match a given property for a node.
   *
   * @param NodeActor node
   * @param string property
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *
   * @returns a JSON object with the following form:
   *   {
   *     // An ordered list of rules that apply
   *     matched: [{
   *       rule: <rule actorid>,
   *       sourceText: <string>, // The source of the selector, relative
   *                             // to the node in question.
   *       selector: <string>, // the selector ID that matched
   *       value: <string>, // the value of the property
   *       status: <int>,
   *         // The status of the match - high numbers are better placed
   *         // to provide styling information:
   *         // 3: Best match, was used.
   *         // 2: Matched, but was overridden.
   *         // 1: Rule from a parent matched.
   *         // 0: Unmatched (never returned in this API)
   *     }, ...],
   *
   *     // The full form of any domrule referenced.
   *     rules: [ <domrule>, ... ], // The full form of any domrule referenced
   *
   *     // The full form of any sheets referenced.
   *     sheets: [ <domsheet>, ... ]
   *  }
   */
  getMatchedSelectors: function (node, property, options) {
    this.cssLogic.sourceFilter = options.filter || CssLogic.FILTER.UA;
    this.cssLogic.highlight(node.rawNode);

    let rules = new Set();
    let sheets = new Set();

    let matched = [];
    let propInfo = this.cssLogic.getPropertyInfo(property);
    for (let selectorInfo of propInfo.matchedSelectors) {
      let cssRule = selectorInfo.selector.cssRule;
      let domRule = cssRule.sourceElement || cssRule.domRule;

      let rule = this._styleRef(domRule);
      rules.add(rule);

      matched.push({
        rule: rule,
        sourceText: this.getSelectorSource(selectorInfo, node.rawNode),
        selector: selectorInfo.selector.text,
        name: selectorInfo.property,
        value: selectorInfo.value,
        status: selectorInfo.status
      });
    }

    this.expandSets(rules, sheets);

    return {
      matched: matched,
      rules: [...rules],
      sheets: [...sheets]
    };
  },

  // Get a selector source for a CssSelectorInfo relative to a given
  // node.
  getSelectorSource: function (selectorInfo, relativeTo) {
    let result = selectorInfo.selector.text;
    if (selectorInfo.elementStyle) {
      let source = selectorInfo.sourceElement;
      if (source === relativeTo) {
        result = "this";
      } else {
        result = CssLogic.getShortName(source);
      }
      result += ".style";
    }
    return result;
  },

  /**
   * Get the set of styles that apply to a given node.
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `inherited`: Include styles inherited from parent nodes.
   *   `matchedSelectors`: Include an array of specific selectors that
   *     caused this rule to match its node.
   */
  getApplied: Task.async(function* (node, options) {
    if (!node) {
      return {entries: [], rules: [], sheets: []};
    }

    this.cssLogic.highlight(node.rawNode);
    let entries = [];
    entries = entries.concat(this._getAllElementRules(node, undefined,
                                                      options));

    let result = this.getAppliedProps(node, entries, options);
    for (let rule of result.rules) {
      // See the comment in |form| to understand this.
      yield rule.getAuthoredCssText();
    }
    return result;
  }),

  _hasInheritedProps: function (style) {
    return Array.prototype.some.call(style, prop => {
      return DOMUtils.isInheritedProperty(prop);
    });
  },

  isPositionEditable: Task.async(function* (node) {
    if (!node || node.rawNode.nodeType !== node.rawNode.ELEMENT_NODE) {
      return false;
    }

    let props = getDefinedGeometryProperties(node.rawNode);

    // Elements with only `width` and `height` are currently not considered
    // editable.
    return props.has("top") ||
           props.has("right") ||
           props.has("left") ||
           props.has("bottom");
  }),

  /**
   * Helper function for getApplied, gets all the rules from a given
   * element. See getApplied for documentation on parameters.
   * @param NodeActor node
   * @param bool inherited
   * @param object options

   * @return Array The rules for a given element. Each item in the
   *               array has the following signature:
   *                - rule RuleActor
   *                - isSystem Boolean
   *                - inherited Boolean
   *                - pseudoElement String
   */
  _getAllElementRules: function (node, inherited, options) {
    let {bindingElement, pseudo} =
        CssLogic.getBindingElementAndPseudo(node.rawNode);
    let rules = [];

    if (!bindingElement || !bindingElement.style) {
      return rules;
    }

    let elementStyle = this._styleRef(bindingElement);
    let showElementStyles = !inherited && !pseudo;
    let showInheritedStyles = inherited &&
                              this._hasInheritedProps(bindingElement.style);

    let rule = {
      rule: elementStyle,
      pseudoElement: null,
      isSystem: false,
      inherited: false
    };

    // First any inline styles
    if (showElementStyles) {
      rules.push(rule);
    }

    // Now any inherited styles
    if (showInheritedStyles) {
      rule.inherited = inherited;
      rules.push(rule);
    }

    // Add normal rules.  Typically this is passing in the node passed into the
    // function, unless if that node was ::before/::after.  In which case,
    // it will pass in the parentNode along with "::before"/"::after".
    this._getElementRules(bindingElement, pseudo, inherited, options)
        .forEach(oneRule => {
          // The only case when there would be a pseudo here is
          // ::before/::after, and in this case we want to tell the
          // view that it belongs to the element (which is a
          // _moz_generated_content native anonymous element).
          oneRule.pseudoElement = null;
          rules.push(oneRule);
        });

    // Now any pseudos (except for ::before / ::after, which was handled as
    // a 'normal rule' above.
    if (showElementStyles) {
      for (let readPseudo of PSEUDO_ELEMENTS_TO_READ) {
        this._getElementRules(bindingElement, readPseudo, inherited, options)
            .forEach(oneRule => {
              rules.push(oneRule);
            });
      }
    }

    return rules;
  },

  /**
   * Helper function for _getAllElementRules, returns the rules from a given
   * element. See getApplied for documentation on parameters.
   * @param DOMNode node
   * @param string pseudo
   * @param DOMNode inherited
   * @param object options
   *
   * @returns Array
   */
  _getElementRules: function (node, pseudo, inherited, options) {
    let domRules = DOMUtils.getCSSStyleRules(node, pseudo);
    if (!domRules) {
      return [];
    }

    let rules = [];

    // getCSSStyleRules returns ordered from least-specific to
    // most-specific.
    for (let i = domRules.Count() - 1; i >= 0; i--) {
      let domRule = domRules.GetElementAt(i);

      let isSystem = !CssLogic.isContentStylesheet(domRule.parentStyleSheet);

      if (isSystem && options.filter != CssLogic.FILTER.UA) {
        continue;
      }

      if (inherited) {
        // Don't include inherited rules if none of its properties
        // are inheritable.
        let hasInherited = [...domRule.style].some(
          prop => DOMUtils.isInheritedProperty(prop)
        );
        if (!hasInherited) {
          continue;
        }
      }

      let ruleActor = this._styleRef(domRule);
      rules.push({
        rule: ruleActor,
        inherited: inherited,
        isSystem: isSystem,
        pseudoElement: pseudo
      });
    }
    return rules;
  },

  /**
   * Given a node and a CSS rule, walk up the DOM looking for a
   * matching element rule.  Return an array of all found entries, in
   * the form generated by _getAllElementRules.  Note that this will
   * always return an array of either zero or one element.
   *
   * @param {NodeActor} node the node
   * @param {CSSStyleRule} filterRule the rule to filter for
   * @return {Array} array of zero or one elements; if one, the element
   *                 is the entry as returned by _getAllElementRules.
   */
  findEntryMatchingRule: function (node, filterRule) {
    const options = {matchedSelectors: true, inherited: true};
    let entries = [];
    let parent = this.walker.parentNode(node);
    while (parent && parent.rawNode.nodeType != Ci.nsIDOMNode.DOCUMENT_NODE) {
      entries = entries.concat(this._getAllElementRules(parent, parent,
                                                        options));
      parent = this.walker.parentNode(parent);
    }

    return entries.filter(entry => entry.rule.rawRule === filterRule);
  },

  /**
   * Helper function for getApplied that fetches a set of style properties that
   * apply to the given node and associated rules
   * @param NodeActor node
   * @param object options
   *   `filter`: A string filter that affects the "matched" handling.
   *     'user': Include properties from user style sheets.
   *     'ua': Include properties from user and user-agent sheets.
   *     Default value is 'ua'
   *   `inherited`: Include styles inherited from parent nodes.
   *   `matchedSelectors`: Include an array of specific selectors that
   *     caused this rule to match its node.
   * @param array entries
   *   List of appliedstyle objects that lists the rules that apply to the
   *   node. If adding a new rule to the stylesheet, only the new rule entry
   *   is provided and only the style properties that apply to the new
   *   rule is fetched.
   * @returns Object containing the list of rule entries, rule actors and
   *   stylesheet actors that applies to the given node and its associated
   *   rules.
   */
  getAppliedProps: function (node, entries, options) {
    if (options.inherited) {
      let parent = this.walker.parentNode(node);
      while (parent && parent.rawNode.nodeType != Ci.nsIDOMNode.DOCUMENT_NODE) {
        entries = entries.concat(this._getAllElementRules(parent, parent,
                                                          options));
        parent = this.walker.parentNode(parent);
      }
    }

    if (options.matchedSelectors) {
      for (let entry of entries) {
        if (entry.rule.type === ELEMENT_STYLE) {
          continue;
        }

        let domRule = entry.rule.rawRule;
        let selectors = CssLogic.getSelectors(domRule);
        let element = entry.inherited ? entry.inherited.rawNode : node.rawNode;

        let {bindingElement, pseudo} =
            CssLogic.getBindingElementAndPseudo(element);
        entry.matchedSelectors = [];
        for (let i = 0; i < selectors.length; i++) {
          if (DOMUtils.selectorMatchesElement(bindingElement, domRule, i,
                                              pseudo)) {
            entry.matchedSelectors.push(selectors[i]);
          }
        }
      }
    }

    // Add all the keyframes rule associated with the element
    let computedStyle = this.cssLogic.computedStyle;
    if (computedStyle) {
      let animationNames = computedStyle.animationName.split(",");
      animationNames = animationNames.map(name => name.trim());

      if (animationNames) {
        // Traverse through all the available keyframes rule and add
        // the keyframes rule that matches the computed animation name
        for (let keyframesRule of this.cssLogic.keyframesRules) {
          if (animationNames.indexOf(keyframesRule.name) > -1) {
            for (let rule of keyframesRule.cssRules) {
              entries.push({
                rule: this._styleRef(rule),
                keyframes: this._styleRef(keyframesRule)
              });
            }
          }
        }
      }
    }

    let rules = new Set();
    let sheets = new Set();
    entries.forEach(entry => rules.add(entry.rule));
    this.expandSets(rules, sheets);

    return {
      entries: entries,
      rules: [...rules],
      sheets: [...sheets]
    };
  },

  /**
   * Expand Sets of rules and sheets to include all parent rules and sheets.
   */
  expandSets: function (ruleSet, sheetSet) {
    // Sets include new items in their iteration
    for (let rule of ruleSet) {
      if (rule.rawRule.parentRule) {
        let parent = this._styleRef(rule.rawRule.parentRule);
        if (!ruleSet.has(parent)) {
          ruleSet.add(parent);
        }
      }
      if (rule.rawRule.parentStyleSheet) {
        let parent = this._sheetRef(rule.rawRule.parentStyleSheet);
        if (!sheetSet.has(parent)) {
          sheetSet.add(parent);
        }
      }
    }

    for (let sheet of sheetSet) {
      if (sheet.rawSheet.parentStyleSheet) {
        let parent = this._sheetRef(sheet.rawSheet.parentStyleSheet);
        if (!sheetSet.has(parent)) {
          sheetSet.add(parent);
        }
      }
    }
  },

  /**
   * Get layout-related information about a node.
   * This method returns an object with properties giving information about
   * the node's margin, border, padding and content region sizes, as well
   * as information about the type of box, its position, z-index, etc...
   * @param {NodeActor} node
   * @param {Object} options The only available option is autoMargins.
   * If set to true, the element's margins will receive an extra check to see
   * whether they are set to "auto" (knowing that the computed-style in this
   * case would return "0px").
   * The returned object will contain an extra property (autoMargins) listing
   * all margins that are set to auto, e.g. {top: "auto", left: "auto"}.
   * @return {Object}
   */
  getLayout: function (node, options) {
    this.cssLogic.highlight(node.rawNode);

    let layout = {};

    // First, we update the first part of the layout view, with
    // the size of the element.

    let clientRect = node.rawNode.getBoundingClientRect();
    layout.width = parseFloat(clientRect.width.toPrecision(6));
    layout.height = parseFloat(clientRect.height.toPrecision(6));

    // We compute and update the values of margins & co.
    let style = CssLogic.getComputedStyle(node.rawNode);
    for (let prop of [
      "position",
      "margin-top",
      "margin-right",
      "margin-bottom",
      "margin-left",
      "padding-top",
      "padding-right",
      "padding-bottom",
      "padding-left",
      "border-top-width",
      "border-right-width",
      "border-bottom-width",
      "border-left-width",
      "z-index",
      "box-sizing",
      "display"
    ]) {
      layout[prop] = style.getPropertyValue(prop);
    }

    if (options.autoMargins) {
      layout.autoMargins = this.processMargins(this.cssLogic);
    }

    for (let i in this.map) {
      let property = this.map[i].property;
      this.map[i].value = parseFloat(style.getPropertyValue(property));
    }

    return layout;
  },

  /**
   * Find 'auto' margin properties.
   */
  processMargins: function (cssLogic) {
    let margins = {};

    for (let prop of ["top", "bottom", "left", "right"]) {
      let info = cssLogic.getPropertyInfo("margin-" + prop);
      let selectors = info.matchedSelectors;
      if (selectors && selectors.length > 0 && selectors[0].value == "auto") {
        margins[prop] = "auto";
      }
    }

    return margins;
  },

  /**
   * On page navigation, tidy up remaining objects.
   */
  onFrameUnload: function () {
    this.styleElements = new WeakMap();
  },

  /**
   * Helper function to addNewRule to get or create a style tag in the provided
   * document.
   *
   * @param {Document} document
   *        The document in which the style element should be appended.
   * @returns DOMElement of the style tag
   */
  getStyleElement: function (document) {
    if (!this.styleElements.has(document)) {
      let style = document.createElementNS(XHTML_NS, "style");
      style.setAttribute("type", "text/css");
      document.documentElement.appendChild(style);
      this.styleElements.set(document, style);
    }

    return this.styleElements.get(document);
  },

  /**
   * Helper function for adding a new rule and getting its applied style
   * properties
   * @param NodeActor node
   * @param CSSStyleRule rule
   * @returns Object containing its applied style properties
   */
  getNewAppliedProps: function (node, rule) {
    let ruleActor = this._styleRef(rule);
    return this.getAppliedProps(node, [{ rule: ruleActor }],
      { matchedSelectors: true });
  },

  /**
   * Adds a new rule, and returns the new StyleRuleActor.
   * @param {NodeActor} node
   * @param {String} pseudoClasses The list of pseudo classes to append to the
   *        new selector.
   * @param {Boolean} editAuthored
   *        True if the selector should be updated by editing the
   *        authored text; false if the selector should be updated via
   *        CSSOM.
   * @returns {StyleRuleActor} the new rule
   */
  addNewRule: Task.async(function* (node, pseudoClasses, editAuthored = false) {
    let style = this.getStyleElement(node.rawNode.ownerDocument);
    let sheet = style.sheet;
    let cssRules = sheet.cssRules;
    let rawNode = node.rawNode;
    let classes = [...rawNode.classList];

    let selector;
    if (rawNode.id) {
      selector = "#" + CSS.escape(rawNode.id);
    } else if (classes.length > 0) {
      selector = "." + classes.map(c => CSS.escape(c)).join(".");
    } else {
      selector = rawNode.localName;
    }

    if (pseudoClasses && pseudoClasses.length > 0) {
      selector += pseudoClasses.join("");
    }

    let index = sheet.insertRule(selector + " {}", cssRules.length);

    // If inserting the rule succeeded, go ahead and edit the source
    // text if requested.
    if (editAuthored) {
      let sheetActor = this._sheetRef(sheet);
      let {str: authoredText} = yield sheetActor.getText();
      authoredText += "\n" + selector + " {\n" + "}";
      yield sheetActor.update(authoredText, false);
    }

    return this.getNewAppliedProps(node, sheet.cssRules.item(index));
  })
});
exports.PageStyleActor = PageStyleActor;

/**
 * An actor that represents a CSS style object on the protocol.
 *
 * We slightly flatten the CSSOM for this actor, it represents
 * both the CSSRule and CSSStyle objects in one actor.  For nodes
 * (which have a CSSStyle but no CSSRule) we create a StyleRuleActor
 * with a special rule type (100).
 */
var StyleRuleActor = protocol.ActorClassWithSpec(styleRuleSpec, {
  initialize: function (pageStyle, item) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.pageStyle = pageStyle;
    this.rawStyle = item.style;
    this._parentSheet = null;
    this._onStyleApplied = this._onStyleApplied.bind(this);

    if (item instanceof (Ci.nsIDOMCSSRule)) {
      this.type = item.type;
      this.rawRule = item;
      if ((this.type === Ci.nsIDOMCSSRule.STYLE_RULE ||
           this.type === Ci.nsIDOMCSSRule.KEYFRAME_RULE) &&
          this.rawRule.parentStyleSheet) {
        this.line = DOMUtils.getRelativeRuleLine(this.rawRule);
        this.column = DOMUtils.getRuleColumn(this.rawRule);
        this._parentSheet = this.rawRule.parentStyleSheet;
        this._computeRuleIndex();
        this.sheetActor = this.pageStyle._sheetRef(this._parentSheet);
        this.sheetActor.on("style-applied", this._onStyleApplied);
      }
    } else {
      // Fake a rule
      this.type = ELEMENT_STYLE;
      this.rawNode = item;
      this.rawRule = {
        style: item.style,
        toString: function () {
          return "[element rule " + this.style + "]";
        }
      };
    }
  },

  get conn() {
    return this.pageStyle.conn;
  },

  destroy: function () {
    if (!this.rawStyle) {
      return;
    }
    protocol.Actor.prototype.destroy.call(this);
    this.rawStyle = null;
    this.pageStyle = null;
    this.rawNode = null;
    this.rawRule = null;
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
    return this.type === ELEMENT_STYLE ||
           (this._parentSheet &&
            // If a rule does not have source, then it has been modified via
            // CSSOM; and we should fall back to non-authored editing.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1224121
            this.sheetActor.allRulesHaveSource() &&
            // Special case about:PreferenceStyleSheet, as it is generated on
            // the fly and the URI is not registered with the about:handler
            // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
            this._parentSheet.href !== "about:PreferenceStyleSheet");
  },

  getDocument: function (sheet) {
    let document;

    if (sheet.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
      document = sheet.ownerNode;
    } else {
      document = sheet.ownerNode.ownerDocument;
    }

    return document;
  },

  toString: function () {
    return "[StyleRuleActor for " + this.rawRule + "]";
  },

  form: function (detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,
      type: this.type,
      line: this.line || undefined,
      column: this.column,
      traits: {
        // Whether the style rule actor implements the modifySelector2 method
        // that allows for unmatched rule to be added
        modifySelectorUnmatched: true,
        // Whether the style rule actor implements the setRuleText
        // method.
        canSetRuleText: this.canSetRuleText,
      }
    };

    if (this.rawRule.parentRule) {
      form.parentRule =
        this.pageStyle._styleRef(this.rawRule.parentRule).actorID;

      // CSS rules that we call media rules are STYLE_RULES that are children
      // of MEDIA_RULEs. We need to check the parentRule to check if a rule is
      // a media rule so we do this here instead of in the switch statement
      // below.
      if (this.rawRule.parentRule.type === Ci.nsIDOMCSSRule.MEDIA_RULE) {
        form.media = [];
        for (let i = 0, n = this.rawRule.parentRule.media.length; i < n; i++) {
          form.media.push(this.rawRule.parentRule.media.item(i));
        }
      }
    }
    if (this._parentSheet) {
      form.parentStyleSheet =
        this.pageStyle._sheetRef(this._parentSheet).actorID;
    }

    // One tricky thing here is that other methods in this actor must
    // ensure that authoredText has been set before |form| is called.
    // This has to be treated specially, for now, because we cannot
    // synchronously compute the authored text, but |form| also cannot
    // return a promise.  See bug 1205868.
    form.authoredText = this.authoredText;

    switch (this.type) {
      case Ci.nsIDOMCSSRule.STYLE_RULE:
        form.selectors = CssLogic.getSelectors(this.rawRule);
        form.cssText = this.rawStyle.cssText || "";
        break;
      case ELEMENT_STYLE:
        // Elements don't have a parent stylesheet, and therefore
        // don't have an associated URI.  Provide a URI for
        // those.
        let doc = this.rawNode.ownerDocument;
        form.href = doc.location ? doc.location.href : "";
        form.cssText = this.rawStyle.cssText || "";
        form.authoredText = this.rawNode.getAttribute("style");
        break;
      case Ci.nsIDOMCSSRule.CHARSET_RULE:
        form.encoding = this.rawRule.encoding;
        break;
      case Ci.nsIDOMCSSRule.IMPORT_RULE:
        form.href = this.rawRule.href;
        break;
      case Ci.nsIDOMCSSRule.KEYFRAMES_RULE:
        form.cssText = this.rawRule.cssText;
        form.name = this.rawRule.name;
        break;
      case Ci.nsIDOMCSSRule.KEYFRAME_RULE:
        form.cssText = this.rawStyle.cssText || "";
        form.keyText = this.rawRule.keyText || "";
        break;
    }

    // Parse the text into a list of declarations so the client doesn't have to
    // and so that we can safely determine if a declaration is valid rather than
    // have the client guess it.
    if (form.authoredText || form.cssText) {
      let declarations = parseDeclarations(isCssPropertyKnown,
                                           form.authoredText || form.cssText,
                                           true);
      form.declarations = declarations.map(decl => {
        decl.isValid = DOMUtils.cssPropertyIsValid(decl.name, decl.value);
        return decl;
      });
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
  _notifyLocationChanged: function (line, column) {
    events.emit(this, "location-changed", line, column);
  },

  /**
   * Compute the index of this actor's raw rule in its parent style
   * sheet.  The index is a vector where each element is the index of
   * a given CSS rule in its parent.  A vector is used to support
   * nested rules.
   */
  _computeRuleIndex: function () {
    let rule = this.rawRule;
    let result = [];

    while (rule) {
      let cssRules;
      if (rule.parentRule) {
        cssRules = rule.parentRule.cssRules;
      } else {
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
  _getRuleFromIndex: function (parentSheet) {
    let currentRule = null;
    for (let i of this._ruleIndex) {
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
  _onStyleApplied: function (kind) {
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
      let oldRule = this.rawRule;
      this.rawRule = this._getRuleFromIndex(this._parentSheet);
      // Also tell the page style so that future calls to _styleRef
      // return the same StyleRuleActor.
      this.pageStyle.updateStyleRef(oldRule, this.rawRule, this);
      let line = DOMUtils.getRelativeRuleLine(this.rawRule);
      let column = DOMUtils.getRuleColumn(this.rawRule);
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
   */
  getAuthoredCssText: function () {
    if (!this.canSetRuleText ||
        (this.type !== Ci.nsIDOMCSSRule.STYLE_RULE &&
         this.type !== Ci.nsIDOMCSSRule.KEYFRAME_RULE)) {
      return promise.resolve("");
    }

    if (typeof this.authoredText === "string") {
      return promise.resolve(this.authoredText);
    }

    let parentStyleSheet =
        this.pageStyle._sheetRef(this._parentSheet);
    return parentStyleSheet.getText().then((longStr) => {
      let cssText = longStr.str;
      let {text} = getRuleText(cssText, this.line, this.column);

      // Cache the result on the rule actor to avoid parsing again next time
      this.authoredText = text;
      return this.authoredText;
    });
  },

  /**
   * Set the contents of the rule.  This rewrites the rule in the
   * stylesheet and causes it to be re-evaluated.
   *
   * @param {String} newText the new text of the rule
   * @returns the rule with updated properties
   */
  setRuleText: Task.async(function* (newText) {
    if (!this.canSetRuleText) {
      throw new Error("invalid call to setRuleText");
    }

    if (this.type === ELEMENT_STYLE) {
      // For element style rules, set the node's style attribute.
      this.rawNode.setAttribute("style", newText);
    } else {
      // For stylesheet rules, set the text in the stylesheet.
      let parentStyleSheet = this.pageStyle._sheetRef(this._parentSheet);
      let {str: cssText} = yield parentStyleSheet.getText();

      let {offset, text} = getRuleText(cssText, this.line, this.column);
      cssText = cssText.substring(0, offset) + newText +
        cssText.substring(offset + text.length);

      yield parentStyleSheet.update(cssText, false, UPDATE_PRESERVING_RULES);
    }

    this.authoredText = newText;

    return this;
  }),

  /**
   * Modify a rule's properties. Passed an array of modifications:
   * {
   *   type: "set",
   *   name: <string>,
   *   value: <string>,
   *   priority: <optional string>
   * }
   *  or
   * {
   *   type: "remove",
   *   name: <string>,
   * }
   *
   * @returns the rule with updated properties
   */
  modifyProperties: function (modifications) {
    // Use a fresh element for each call to this function to prevent side
    // effects that pop up based on property values that were already set on the
    // element.

    let document;
    if (this.rawNode) {
      document = this.rawNode.ownerDocument;
    } else {
      let parentStyleSheet = this._parentSheet;
      while (parentStyleSheet.ownerRule &&
          parentStyleSheet.ownerRule instanceof Ci.nsIDOMCSSImportRule) {
        parentStyleSheet = parentStyleSheet.ownerRule.parentStyleSheet;
      }

      document = this.getDocument(parentStyleSheet);
    }

    let tempElement = document.createElementNS(XHTML_NS, "div");

    for (let mod of modifications) {
      if (mod.type === "set") {
        tempElement.style.setProperty(mod.name, mod.value, mod.priority || "");
        this.rawStyle.setProperty(mod.name,
          tempElement.style.getPropertyValue(mod.name), mod.priority || "");
      } else if (mod.type === "remove") {
        this.rawStyle.removeProperty(mod.name);
      }
    }

    return this;
  },

  /**
   * Helper function for modifySelector and modifySelector2, inserts the new
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
  _addNewSelector: Task.async(function* (value, editAuthored) {
    let rule = this.rawRule;
    let parentStyleSheet = this._parentSheet;

    // We know the selector modification is ok, so if the client asked
    // for the authored text to be edited, do it now.
    if (editAuthored) {
      let document = this.getDocument(this._parentSheet);
      try {
        document.querySelector(value);
      } catch (e) {
        return null;
      }

      let sheetActor = this.pageStyle._sheetRef(parentStyleSheet);
      let {str: authoredText} = yield sheetActor.getText();
      let [startOffset, endOffset] = getSelectorOffsets(authoredText, this.line,
                                                        this.column);
      authoredText = authoredText.substring(0, startOffset) + value +
        authoredText.substring(endOffset);
      yield sheetActor.update(authoredText, false, UPDATE_PRESERVING_RULES);
    } else {
      let cssRules = parentStyleSheet.cssRules;
      let cssText = rule.cssText;
      let selectorText = rule.selectorText;

      for (let i = 0; i < cssRules.length; i++) {
        if (rule === cssRules.item(i)) {
          try {
            // Inserts the new style rule into the current style sheet and
            // delete the current rule
            let ruleText = cssText.slice(selectorText.length).trim();
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
  }),

  /**
   * Modify the current rule's selector by inserting a new rule with the new
   * selector value and removing the current rule.
   *
   * Note this method was kept for backward compatibility, but unmatched rules
   * support was added in FF41.
   *
   * @param string value
   *        The new selector value
   * @returns boolean
   *        Returns a boolean if the selector in the stylesheet was modified,
   *        and false otherwise
   */
  modifySelector: Task.async(function* (value) {
    if (this.type === ELEMENT_STYLE) {
      return false;
    }

    let document = this.getDocument(this._parentSheet);
    // Extract the selector, and pseudo elements and classes
    let [selector] = value.split(/(:{1,2}.+$)/);
    let selectorElement;

    try {
      selectorElement = document.querySelector(selector);
    } catch (e) {
      return false;
    }

    // Check if the selector is valid and not the same as the original
    // selector
    if (selectorElement && this.rawRule.selectorText !== value) {
      yield this._addNewSelector(value, false);
      return true;
    }
    return false;
  }),

  /**
   * Modify the current rule's selector by inserting a new rule with the new
   * selector value and removing the current rule.
   *
   * In contrast with the modifySelector method which was used before FF41,
   * this method also returns information about the new rule and applied style
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
  modifySelector2: function (node, value, editAuthored = false) {
    if (this.type === ELEMENT_STYLE ||
        this.rawRule.selectorText === value) {
      return { ruleProps: null, isMatching: true };
    }

    let selectorPromise = this._addNewSelector(value, editAuthored);

    if (editAuthored) {
      selectorPromise = selectorPromise.then((newCssRule) => {
        if (newCssRule) {
          let style = this.pageStyle._styleRef(newCssRule);
          // See the comment in |form| to understand this.
          return style.getAuthoredCssText().then(() => newCssRule);
        }
        return newCssRule;
      });
    }

    return selectorPromise.then((newCssRule) => {
      let ruleProps = null;
      let isMatching = false;

      if (newCssRule) {
        let ruleEntry = this.pageStyle.findEntryMatchingRule(node, newCssRule);
        if (ruleEntry.length === 1) {
          ruleProps =
            this.pageStyle.getAppliedProps(node, ruleEntry,
                                           { matchedSelectors: true });
        } else {
          ruleProps = this.pageStyle.getNewAppliedProps(node, newCssRule);
        }

        isMatching = ruleProps.entries.some((ruleProp) =>
          ruleProp.matchedSelectors.length > 0);
      }

      return { ruleProps, isMatching };
    });
  }
});

/**
 * Helper function for getting an image preview of the given font.
 *
 * @param font {string}
 *        Name of font to preview
 * @param doc {Document}
 *        Document to use to render font
 * @param options {object}
 *        Object with options 'previewText' and 'previewFontSize'
 *
 * @return dataUrl
 *         The data URI of the font preview image
 */
function getFontPreviewData(font, doc, options) {
  options = options || {};
  let previewText = options.previewText || FONT_PREVIEW_TEXT;
  let previewFontSize = options.previewFontSize || FONT_PREVIEW_FONT_SIZE;
  let fillStyle = options.fillStyle || FONT_PREVIEW_FILLSTYLE;
  let fontStyle = options.fontStyle || "";

  let canvas = doc.createElementNS(XHTML_NS, "canvas");
  let ctx = canvas.getContext("2d");
  let fontValue = fontStyle + " " + previewFontSize + "px " + font + ", serif";

  // Get the correct preview text measurements and set the canvas dimensions
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;
  let textWidth = ctx.measureText(previewText).width;

  canvas.width = textWidth * 2 + FONT_PREVIEW_OFFSET * 2;
  canvas.height = previewFontSize * 3;

  // we have to reset these after changing the canvas size
  ctx.font = fontValue;
  ctx.fillStyle = fillStyle;

  // Oversample the canvas for better text quality
  ctx.textBaseline = "top";
  ctx.scale(2, 2);
  ctx.fillText(previewText,
               FONT_PREVIEW_OFFSET,
               Math.round(previewFontSize / 3));

  let dataURL = canvas.toDataURL("image/png");

  return {
    dataURL: dataURL,
    size: textWidth + FONT_PREVIEW_OFFSET * 2
  };
}

exports.getFontPreviewData = getFontPreviewData;

/**
 * Get the text content of a rule given some CSS text, a line and a column
 * Consider the following example:
 * body {
 *  color: red;
 * }
 * p {
 *  line-height: 2em;
 *  color: blue;
 * }
 * Calling the function with the whole text above and line=4 and column=1 would
 * return "line-height: 2em; color: blue;"
 * @param {String} initialText
 * @param {Number} line (1-indexed)
 * @param {Number} column (1-indexed)
 * @return {object} An object of the form {offset: number, text: string}
 *                  The offset is the index into the input string where
 *                  the rule text started.  The text is the content of
 *                  the rule.
 */
function getRuleText(initialText, line, column) {
  if (typeof line === "undefined" || typeof column === "undefined") {
    throw new Error("Location information is missing");
  }

  let {offset: textOffset, text} =
      getTextAtLineColumn(initialText, line, column);
  let lexer = DOMUtils.getCSSLexer(text);

  // Search forward for the opening brace.
  while (true) {
    let token = lexer.nextToken();
    if (!token) {
      throw new Error("couldn't find start of the rule");
    }
    if (token.tokenType === "symbol" && token.text === "{") {
      break;
    }
  }

  // Now collect text until we see the matching close brace.
  let braceDepth = 1;
  let startOffset, endOffset;
  while (true) {
    let token = lexer.nextToken();
    if (!token) {
      break;
    }
    if (startOffset === undefined) {
      startOffset = token.startOffset;
    }
    if (token.tokenType === "symbol") {
      if (token.text === "{") {
        ++braceDepth;
      } else if (token.text === "}") {
        --braceDepth;
        if (braceDepth == 0) {
          break;
        }
      }
    }
    endOffset = token.endOffset;
  }

  // If the rule was of the form "selector {" with no closing brace
  // and no properties, just return an empty string.
  if (startOffset === undefined) {
    return {offset: 0, text: ""};
  }
  // If the input didn't have any tokens between the braces (e.g.,
  // "div {}"), then the endOffset won't have been set yet; so account
  // for that here.
  if (endOffset === undefined) {
    endOffset = startOffset;
  }

  // Note that this approach will preserve comments, despite the fact
  // that cssTokenizer skips them.
  return {offset: textOffset + startOffset,
          text: text.substring(startOffset, endOffset)};
}

exports.getRuleText = getRuleText;

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

  let {offset: textOffset, text} =
      getTextAtLineColumn(initialText, line, column);
  let lexer = DOMUtils.getCSSLexer(text);

  // Search forward for the opening brace.
  let endOffset;
  while (true) {
    let token = lexer.nextToken();
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

/**
 * Return the offset and substring of |text| that starts at the given
 * line and column.
 * @param {String} text
 * @param {Number} line (1-indexed)
 * @param {Number} column (1-indexed)
 * @return {object} An object of the form {offset: number, text: string},
 *                  where the offset is the offset into the input string
 *                  where the text starts, and where text is the text.
 */
function getTextAtLineColumn(text, line, column) {
  let offset;
  if (line > 1) {
    let rx = new RegExp("(?:.*(?:\\r\\n|\\n|\\r|\\f)){" + (line - 1) + "}");
    offset = rx.exec(text)[0].length;
  } else {
    offset = 0;
  }
  offset += column - 1;
  return {offset: offset, text: text.substr(offset) };
}

exports.getTextAtLineColumn = getTextAtLineColumn;
