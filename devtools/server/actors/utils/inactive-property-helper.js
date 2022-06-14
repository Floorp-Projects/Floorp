/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const InspectorUtils = require("InspectorUtils");

loader.lazyRequireGetter(
  this,
  "CssLogic",
  "devtools/server/actors/inspector/css-logic",
  true
);

const INACTIVE_CSS_ENABLED = Services.prefs.getBoolPref(
  "devtools.inspector.inactive.css.enabled",
  false
);

const VISITED_MDN_LINK = "https://developer.mozilla.org/docs/Web/CSS/:visited";
const VISITED_INVALID_PROPERTIES = allCssPropertiesExcept([
  "all",
  "color",
  "background",
  "background-color",
  "border",
  "border-color",
  "border-bottom-color",
  "border-left-color",
  "border-right-color",
  "border-top-color",
  "border-block",
  "border-block-color",
  "border-block-start-color",
  "border-block-end-color",
  "border-inline",
  "border-inline-color",
  "border-inline-start-color",
  "border-inline-end-color",
  "column-rule",
  "column-rule-color",
  "outline",
  "outline-color",
  "text-decoration-color",
  "text-emphasis-color",
]);

// Set of node names which are always treated as replaced elements:
const REPLACED_ELEMENTS_NAMES = new Set([
  "audio",
  "br",
  "button",
  "canvas",
  "embed",
  "hr",
  "iframe",
  // Inputs are generally replaced elements. E.g. checkboxes and radios are replaced
  // unless they have `appearance: none`. However unconditionally treating them
  // as replaced is enough for our purpose here, and avoids extra complexity that
  // will likely not be necessary in most cases.
  "input",
  "math",
  "object",
  "picture",
  // Select is a replaced element if it has `size<=1` or no size specified, but
  // unconditionally treating it as replaced is enough for our purpose here, and
  // avoids extra complexity that will likely not be necessary in most cases.
  "select",
  "svg",
  "textarea",
  "video",
]);

class InactivePropertyHelper {
  /**
   * A list of rules for when CSS properties have no effect.
   *
   * In certain situations, CSS properties do not have any effect. A common
   * example is trying to set a width on an inline element like a <span>.
   *
   * There are so many properties in CSS that it's difficult to remember which
   * ones do and don't apply in certain situations. Some are straight-forward
   * like `flex-wrap` only applying to an element that has `display:flex`.
   * Others are less trivial like setting something other than a color on a
   * `:visited` pseudo-class.
   *
   * This file contains "rules" in the form of objects with the following
   * properties:
   * {
   *   invalidProperties:
   *     Array of CSS property names that are inactive if the rule matches.
   *   when:
   *     The rule itself, a JS function used to identify the conditions
   *     indicating whether a property is valid or not.
   *   fixId:
   *     A Fluent id containing a suggested solution to the problem that is
   *     causing a property to be inactive.
   *   msgId:
   *     A Fluent id containing an error message explaining why a property is
   *     inactive in this situation.
   * }
   *
   * If you add a new rule, also add a test for it in:
   * server/tests/chrome/test_inspector-inactive-property-helper.html
   *
   * The main export is `isPropertyUsed()`, which can be used to check if a
   * property is used or not, and why.
   *
   * NOTE: We should generally *not* add rules here for any CSS properties that
   * inherit by default, because it's hard for us to know whether such
   * properties are truly "inactive". Web developers might legitimately set
   * such a property on any arbitrary element, in order to concisely establish
   * the default property-value throughout that element's subtree. For example,
   * consider the "list-style-*" properties, which inherit by default and which
   * only have a rendering effect on elements with "display:list-item"
   * (e.g. <li>). It might superficially seem like we could add a rule here to
   * warn about usages of these properties on non-"list-item" elements, but we
   * shouldn't actually warn about that. A web developer may legitimately
   * prefer to set these properties on an arbitrary container element (e.g. an
   * <ol> element, or even the <html> element) in order to concisely adjust the
   * rendering of a whole list (or all the lists in a document).
   */
  get VALIDATORS() {
    return [
      // Flex container property used on non-flex container.
      {
        invalidProperties: ["flex-direction", "flex-flow", "flex-wrap"],
        when: () => !this.flexContainer,
        fixId: "inactive-css-not-flex-container-fix",
        msgId: "inactive-css-not-flex-container",
      },
      // Flex item property used on non-flex item.
      {
        invalidProperties: ["flex", "flex-basis", "flex-grow", "flex-shrink"],
        when: () => !this.flexItem,
        fixId: "inactive-css-not-flex-item-fix-2",
        msgId: "inactive-css-not-flex-item",
      },
      // Grid container property used on non-grid container.
      {
        invalidProperties: [
          "grid-auto-columns",
          "grid-auto-flow",
          "grid-auto-rows",
          "grid-template",
          "grid-template-areas",
          "grid-template-columns",
          "grid-template-rows",
          "justify-items",
        ],
        when: () => !this.gridContainer,
        fixId: "inactive-css-not-grid-container-fix",
        msgId: "inactive-css-not-grid-container",
      },
      // Grid item property used on non-grid item.
      {
        invalidProperties: [
          "grid-area",
          "grid-column",
          "grid-column-end",
          "grid-column-start",
          "grid-row",
          "grid-row-end",
          "grid-row-start",
          "justify-self",
        ],
        when: () => !this.gridItem && !this.isAbsPosGridElement(),
        fixId: "inactive-css-not-grid-item-fix-2",
        msgId: "inactive-css-not-grid-item",
      },
      // Grid and flex item properties used on non-grid or non-flex item.
      {
        invalidProperties: ["align-self", "place-self", "order"],
        when: () =>
          !this.gridItem && !this.flexItem && !this.isAbsPosGridElement(),
        fixId: "inactive-css-not-grid-or-flex-item-fix-3",
        msgId: "inactive-css-not-grid-or-flex-item",
      },
      // Grid and flex container properties used on non-grid or non-flex container.
      {
        invalidProperties: [
          "align-items",
          "justify-content",
          "place-content",
          "place-items",
          "row-gap",
          // grid-*-gap are supported legacy shorthands for the corresponding *-gap properties.
          // See https://drafts.csswg.org/css-align-3/#gap-legacy for more information.
          "grid-row-gap",
        ],
        when: () => !this.gridContainer && !this.flexContainer,
        fixId: "inactive-css-not-grid-or-flex-container-fix",
        msgId: "inactive-css-not-grid-or-flex-container",
      },
      // align-content is special as align-content:baseline does have an effect on all
      // grid items, flex items and table cells, regardless of what type of box they are.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1598730
      {
        invalidProperties: ["align-content"],
        when: () =>
          !this.style["align-content"].includes("baseline") &&
          !this.gridContainer &&
          !this.flexContainer,
        fixId: "inactive-css-not-grid-or-flex-container-fix",
        msgId: "inactive-css-not-grid-or-flex-container",
      },
      // column-gap and shorthands used on non-grid or non-flex or non-multi-col container.
      {
        invalidProperties: [
          "column-gap",
          "gap",
          "grid-gap",
          // grid-*-gap are supported legacy shorthands for the corresponding *-gap properties.
          // See https://drafts.csswg.org/css-align-3/#gap-legacy for more information.
          "grid-column-gap",
        ],
        when: () =>
          !this.gridContainer && !this.flexContainer && !this.multiColContainer,
        fixId:
          "inactive-css-not-grid-or-flex-container-or-multicol-container-fix",
        msgId: "inactive-css-not-grid-or-flex-container-or-multicol-container",
      },
      // Inline properties used on non-inline-level elements.
      {
        invalidProperties: ["vertical-align"],
        when: () => {
          const { selectorText } = this.cssRule;

          const isFirstLetter =
            selectorText && selectorText.includes("::first-letter");
          const isFirstLine =
            selectorText && selectorText.includes("::first-line");

          return !this.isInlineLevel() && !isFirstLetter && !isFirstLine;
        },
        fixId: "inactive-css-not-inline-or-tablecell-fix",
        msgId: "inactive-css-not-inline-or-tablecell",
      },
      // (max-|min-)width used on inline elements, table rows, or row groups.
      {
        invalidProperties: ["max-width", "min-width", "width"],
        when: () =>
          this.nonReplacedInlineBox ||
          this.horizontalTableTrack ||
          this.horizontalTableTrackGroup,
        fixId: "inactive-css-non-replaced-inline-or-table-row-or-row-group-fix",
        msgId: "inactive-css-property-because-of-display",
      },
      // (max-|min-)height used on inline elements, table columns, or column groups.
      {
        invalidProperties: ["max-height", "min-height", "height"],
        when: () =>
          this.nonReplacedInlineBox ||
          this.verticalTableTrack ||
          this.verticalTableTrackGroup,
        fixId:
          "inactive-css-non-replaced-inline-or-table-column-or-column-group-fix",
        msgId: "inactive-css-property-because-of-display",
      },
      {
        invalidProperties: ["display"],
        when: () =>
          this.isFloated &&
          this.checkResolvedStyle("display", [
            "inline",
            "inline-block",
            "inline-table",
            "inline-flex",
            "inline-grid",
            "table-cell",
            "table-row",
            "table-row-group",
            "table-header-group",
            "table-footer-group",
            "table-column",
            "table-column-group",
            "table-caption",
          ]),
        fixId: "inactive-css-not-display-block-on-floated-fix",
        msgId: "inactive-css-not-display-block-on-floated",
      },
      // The property is impossible to override due to :visited restriction.
      {
        invalidProperties: VISITED_INVALID_PROPERTIES,
        when: () => this.isVisitedRule(),
        fixId: "learn-more",
        msgId: "inactive-css-property-is-impossible-to-override-in-visited",
        learnMoreURL: VISITED_MDN_LINK,
      },
      // top, right, bottom, left properties used on non positioned boxes.
      {
        invalidProperties: ["top", "right", "bottom", "left"],
        when: () => !this.isPositioned,
        fixId: "inactive-css-position-property-on-unpositioned-box-fix",
        msgId: "inactive-css-position-property-on-unpositioned-box",
      },
      // z-index property used on non positioned boxes that are not grid/flex items.
      {
        invalidProperties: ["z-index"],
        when: () => !this.isPositioned && !this.gridItem && !this.flexItem,
        fixId: "inactive-css-position-property-on-unpositioned-box-fix",
        msgId: "inactive-css-position-property-on-unpositioned-box",
      },
      // text-overflow property used on elements for which overflow is not set to hidden.
      // Note that this validator only checks if overflow:hidden is set on the element.
      // In theory, we should also be checking if the element is a block as this doesn't
      // normally work on inline element. However there are many edge cases that made it
      // impossible for the JS code to determine whether the type of box would support
      // text-overflow. So, rather than risking to show invalid warnings, we decided to
      // only warn when overflow:hidden wasn't set. There is more information about this
      // in this discussion https://phabricator.services.mozilla.com/D62407 and on the bug
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1551578
      {
        invalidProperties: ["text-overflow"],
        when: () => !this.hasInlineOverflow,
        fixId: "inactive-text-overflow-when-no-overflow-fix",
        msgId: "inactive-text-overflow-when-no-overflow",
      },
      // margin properties used on table internal elements.
      {
        invalidProperties: [
          "margin",
          "margin-block",
          "margin-block-end",
          "margin-block-start",
          "margin-bottom",
          "margin-inline",
          "margin-inline-end",
          "margin-inline-start",
          "margin-left",
          "margin-right",
          "margin-top",
        ],
        when: () => this.internalTableElement,
        fixId: "inactive-css-not-for-internal-table-elements-fix",
        msgId: "inactive-css-not-for-internal-table-elements",
      },
      // padding properties used on table internal elements except table cells.
      {
        invalidProperties: [
          "padding",
          "padding-block",
          "padding-block-end",
          "padding-block-start",
          "padding-bottom",
          "padding-inline",
          "padding-inline-end",
          "padding-inline-start",
          "padding-left",
          "padding-right",
          "padding-top",
        ],
        when: () =>
          this.internalTableElement &&
          !this.checkComputedStyle("display", ["table-cell"]),
        fixId:
          "inactive-css-not-for-internal-table-elements-except-table-cells-fix",
        msgId:
          "inactive-css-not-for-internal-table-elements-except-table-cells",
      },
      // table-layout used on non-table elements.
      {
        invalidProperties: ["table-layout"],
        when: () =>
          !this.checkComputedStyle("display", ["table", "inline-table"]),
        fixId: "inactive-css-not-table-fix",
        msgId: "inactive-css-not-table",
      },
      // scroll-padding-* properties used on non-scrollable elements.
      {
        invalidProperties: [
          "scroll-padding",
          "scroll-padding-top",
          "scroll-padding-right",
          "scroll-padding-bottom",
          "scroll-padding-left",
          "scroll-padding-block",
          "scroll-padding-block-end",
          "scroll-padding-block-start",
          "scroll-padding-inline",
          "scroll-padding-inline-end",
          "scroll-padding-inline-start",
        ],
        when: () => !this.isScrollContainer,
        fixId: "inactive-scroll-padding-when-not-scroll-container-fix",
        msgId: "inactive-scroll-padding-when-not-scroll-container",
      },
    ];
  }

  /**
   * Get a list of unique CSS property names for which there are checks
   * for used/unused state.
   *
   * @return {Set}
   *         List of CSS properties
   */
  get invalidProperties() {
    if (!this._invalidProperties) {
      const allProps = this.VALIDATORS.map(v => v.invalidProperties).flat();
      this._invalidProperties = new Set(allProps);
    }

    return this._invalidProperties;
  }

  /**
   * Is this CSS property having any effect on this element?
   *
   * @param {DOMNode} el
   *        The DOM element.
   * @param {Style} elStyle
   *        The computed style for this DOMNode.
   * @param {DOMRule} cssRule
   *        The CSS rule the property is defined in.
   * @param {String} property
   *        The CSS property name.
   *
   * @return {Object} object
   * @return {String} object.display
   *         The element computed display value.
   * @return {String} object.fixId
   *         A Fluent id containing a suggested solution to the problem that is
   *         causing a property to be inactive.
   * @return {String} object.msgId
   *         A Fluent id containing an error message explaining why a property
   *         is inactive in this situation.
   * @return {String} object.property
   *         The inactive property name.
   * @return {String} object.learnMoreURL
   *         An optional link if we need to open an other link than
   *         the default MDN property one.
   * @return {Boolean} object.used
   *         true if the property is used.
   */
  isPropertyUsed(el, elStyle, cssRule, property) {
    // Assume the property is used when:
    // - the Inactive CSS pref is not enabled
    // - the property is not in the list of properties to check
    if (!INACTIVE_CSS_ENABLED || !this.invalidProperties.has(property)) {
      return { used: true };
    }

    let fixId = "";
    let msgId = "";
    let learnMoreURL = null;
    let used = true;

    this.VALIDATORS.some(validator => {
      // First check if this rule cares about this property.
      let isRuleConcerned = false;

      if (validator.invalidProperties) {
        isRuleConcerned = validator.invalidProperties.includes(property);
      }

      if (!isRuleConcerned) {
        return false;
      }

      this.select(el, elStyle, cssRule, property);

      // And then run the validator, gathering the error message if the
      // validator passes.
      if (validator.when()) {
        fixId = validator.fixId;
        msgId = validator.msgId;
        learnMoreURL = validator.learnMoreURL;
        used = false;

        return true;
      }

      return false;
    });

    this.unselect();

    // Accessing elStyle might throws, we wrap it in a try/catch block to avoid test
    // failures.
    let display;
    try {
      display = elStyle ? elStyle.display : null;
    } catch (e) {}

    return {
      display,
      fixId,
      msgId,
      property,
      learnMoreURL,
      used,
    };
  }

  /**
   * Focus on a node.
   *
   * @param {DOMNode} node
   *        Node to focus on.
   */
  select(node, style, cssRule, property) {
    this._node = node;
    this._cssRule = cssRule;
    this._property = property;
    this._style = style;
  }

  /**
   * Clear references to avoid leaks.
   */
  unselect() {
    this._node = null;
    this._cssRule = null;
    this._property = null;
    this._style = null;
  }

  /**
   * Provide a public reference to node.
   */
  get node() {
    return this._node;
  }

  /**
   * Cache and provide node's computed style.
   */
  get style() {
    return this._style;
  }

  /**
   *  Provide a public reference to the css rule.
   */
  get cssRule() {
    return this._cssRule;
  }

  /**
   * Check if the current node's propName is set to one of the values passed in
   * the values array.
   *
   * @param {String} propName
   *        Property name to check.
   * @param {Array} values
   *        Values to compare against.
   */
  checkComputedStyle(propName, values) {
    if (!this.style) {
      return false;
    }
    return values.some(value => this.style[propName] === value);
  }

  /**
   * Check if a rule's propName is set to one of the values passed in the values
   * array.
   *
   * @param {String} propName
   *        Property name to check.
   * @param {Array} values
   *        Values to compare against.
   */
  checkResolvedStyle(propName, values) {
    if (!(this.cssRule && this.cssRule.style)) {
      return false;
    }
    const { style } = this.cssRule;

    return values.some(value => style[propName] === value);
  }

  /**
   *  Check if the current node is an inline-level box.
   */
  isInlineLevel() {
    return this.checkComputedStyle("display", [
      "inline",
      "inline-block",
      "inline-table",
      "inline-flex",
      "inline-grid",
      "table-cell",
      "table-row",
      "table-row-group",
      "table-header-group",
      "table-footer-group",
    ]);
  }

  /**
   * Check if the current node is a flex container i.e. a node that has a style
   * of `display:flex` or `display:inline-flex`.
   */
  get flexContainer() {
    return this.checkComputedStyle("display", ["flex", "inline-flex"]);
  }

  /**
   * Check if the current node is a flex item.
   */
  get flexItem() {
    return this.isFlexItem(this.node);
  }

  /**
   * Check if the current node is a grid container i.e. a node that has a style
   * of `display:grid` or `display:inline-grid`.
   */
  get gridContainer() {
    return this.checkComputedStyle("display", ["grid", "inline-grid"]);
  }

  /**
   * Check if the current node is a grid item.
   */
  get gridItem() {
    return this.isGridItem(this.node);
  }

  /**
   * Check if the current node is a multi-column container, i.e. a node element whose
   * `column-width` or `column-count` property is not `auto`.
   */
  get multiColContainer() {
    const autoColumnWidth = this.checkComputedStyle("column-width", ["auto"]);
    const autoColumnCount = this.checkComputedStyle("column-count", ["auto"]);

    return !autoColumnWidth || !autoColumnCount;
  }

  /**
   * Check if the current node is a table row.
   */
  get tableRow() {
    return this.style && this.style.display === "table-row";
  }

  /**
   * Check if the current node is a table column.
   */
  get tableColumn() {
    return this.style && this.style.display === "table-column";
  }

  /**
   * Check if the current node is an internal table element.
   */
  get internalTableElement() {
    return this.checkComputedStyle("display", [
      "table-cell",
      "table-row",
      "table-row-group",
      "table-header-group",
      "table-footer-group",
      "table-column",
      "table-column-group",
    ]);
  }

  /**
   * Check if the current node is a horizontal table track. That is: either a table row
   * displayed in horizontal writing mode, or a table column displayed in vertical writing
   * mode.
   */
  get horizontalTableTrack() {
    if (!this.tableRow && !this.tableColumn) {
      return false;
    }

    const tableTrackParent = this.getTableTrackParent();

    return this.hasVerticalWritingMode(tableTrackParent)
      ? this.tableColumn
      : this.tableRow;
  }

  /**
   * Check if the current node is a vertical table track. That is: either a table row
   * displayed in vertical writing mode, or a table column displayed in horizontal writing
   * mode.
   */
  get verticalTableTrack() {
    if (!this.tableRow && !this.tableColumn) {
      return false;
    }

    const tableTrackParent = this.getTableTrackParent();

    return this.hasVerticalWritingMode(tableTrackParent)
      ? this.tableRow
      : this.tableColumn;
  }

  /**
   * Check if the current node is a row group.
   */
  get rowGroup() {
    return this.isRowGroup(this.node);
  }

  /**
   * Check if the current node is a table column group.
   */
  get columnGroup() {
    return this.isColumnGroup(this.node);
  }

  /**
   * Check if the current node is a horizontal table track group. That is: either a table
   * row group displayed in horizontal writing mode, or a table column group displayed in
   * vertical writing mode.
   */
  get horizontalTableTrackGroup() {
    if (!this.rowGroup && !this.columnGroup) {
      return false;
    }

    const tableTrackParent = this.getTableTrackParent(true);
    const isVertical = this.hasVerticalWritingMode(tableTrackParent);

    const isHorizontalRowGroup = this.rowGroup && !isVertical;
    const isHorizontalColumnGroup = this.columnGroup && isVertical;

    return isHorizontalRowGroup || isHorizontalColumnGroup;
  }

  /**
   * Check if the current node is a vertical table track group. That is: either a table row
   * group displayed in vertical writing mode, or a table column group displayed in
   * horizontal writing mode.
   */
  get verticalTableTrackGroup() {
    if (!this.rowGroup && !this.columnGroup) {
      return false;
    }

    const tableTrackParent = this.getTableTrackParent(true);
    const isVertical = this.hasVerticalWritingMode(tableTrackParent);

    const isVerticalRowGroup = this.rowGroup && isVertical;
    const isVerticalColumnGroup = this.columnGroup && !isVertical;

    return isVerticalRowGroup || isVerticalColumnGroup;
  }

  /**
   * Returns whether this element uses CSS layout.
   */
  get hasCssLayout() {
    return !this.isSvg && !this.isMathMl;
  }

  /**
   * Check if the current node is a non-replaced CSS inline box.
   */
  get nonReplacedInlineBox() {
    return (
      this.hasCssLayout &&
      this.nonReplaced &&
      this.style &&
      this.style.display === "inline"
    );
  }

  /**
   * Check if the current node is a non-replaced element. See `replaced()` for
   * a description of what a replaced element is.
   */
  get nonReplaced() {
    return !this.replaced;
  }

  /**
   * Check if the current node is an absolutely-positioned element.
   */
  get isAbsolutelyPositioned() {
    return this.checkComputedStyle("position", ["absolute", "fixed"]);
  }

  /**
   * Check if the current node is positioned (i.e. its position property has a value other
   * than static).
   */
  get isPositioned() {
    return this.checkComputedStyle("position", [
      "relative",
      "absolute",
      "fixed",
      "sticky",
    ]);
  }

  /**
   * Check if the current node is floated
   */
  get isFloated() {
    return this.style && this.style.cssFloat !== "none";
  }

  /**
   * Check if the current node is scrollable
   */
  get isScrollContainer() {
    // If `overflow` doesn't contain the values `visible` or `clip`, it is a scroll container.
    // While `hidden` doesn't allow scrolling via a user interaction, the element can
    // still be scrolled programmatically.
    // See https://www.w3.org/TR/css-overflow-3/#overflow-properties.
    const overflow = computedStyle(this.node).overflow;
    // `overflow` is a shorthand for `overflow-x` and `overflow-y`
    // (and with that also for `overflow-inline` and `overflow-block`),
    // so may hold two values.
    const overflowValues = overflow.split(" ");
    return !(
      overflowValues.includes("visible") || overflowValues.includes("clip")
    );
  }

  /**
   * Check if the current node has inline overflow
   */
  get hasInlineOverflow() {
    const property = this.hasVerticalWritingMode(this.node)
      ? "overflow-y"
      : "overflow-x";

    return !this.checkComputedStyle(property, ["visible"]);
  }

  /**
   * Check if the current node is a replaced element i.e. an element with
   * content that will be replaced e.g. <img>, <audio>, <video> or <object>
   * elements.
   */
  get replaced() {
    if (REPLACED_ELEMENTS_NAMES.has(this.localName)) {
      return true;
    }

    // img tags are replaced elements only when the image has finished loading.
    if (this.localName === "img" && this.node.complete) {
      return true;
    }

    return false;
  }

  /**
   * Return the current node's localName.
   *
   * @returns {String}
   */
  get localName() {
    return this.node.localName;
  }

  /**
   * Return whether the node is a MathML element.
   */
  get isMathMl() {
    return this.node.namespaceURI === "http://www.w3.org/1998/Math/MathML";
  }

  /**
   * Return whether the node is an SVG element.
   */
  get isSvg() {
    return this.node.namespaceURI === "http://www.w3.org/2000/svg";
  }

  /**
   * Check if the current node is an absolutely-positioned grid element.
   * See: https://drafts.csswg.org/css-grid/#abspos-items
   *
   * @return {Boolean} whether or not the current node is absolutely-positioned by a
   *                   grid container.
   */
  isAbsPosGridElement() {
    if (!this.isAbsolutelyPositioned) {
      return false;
    }

    const containingBlock = this.getContainingBlock();

    return containingBlock !== null && this.isGridContainer(containingBlock);
  }

  /**
   * Check if a node is a flex item.
   *
   * @param {DOMNode} node
   *        The node to check.
   */
  isFlexItem(node) {
    return !!node.parentFlexElement;
  }

  /**
   * Check if a node is a flex container.
   *
   * @param {DOMNode} node
   *        The node to check.
   */
  isFlexContainer(node) {
    return !!node.getAsFlexContainer();
  }

  /**
   * Check if a node is a grid container.
   *
   * @param {DOMNode} node
   *        The node to check.
   */
  isGridContainer(node) {
    return node.hasGridFragments();
  }

  /**
   * Check if a node is a grid item.
   *
   * @param {DOMNode} node
   *        The node to check.
   */
  isGridItem(node) {
    return !!this.getParentGridElement(this.node);
  }

  isVisitedRule() {
    if (!CssLogic.hasVisitedState(this.node)) {
      return false;
    }

    const selectors = CssLogic.getSelectors(this.cssRule);
    if (!selectors.some(s => s.endsWith(":visited"))) {
      return false;
    }

    const { bindingElement, pseudo } = CssLogic.getBindingElementAndPseudo(
      this.node
    );

    for (let i = 0; i < selectors.length; i++) {
      if (
        !selectors[i].endsWith(":visited") &&
        InspectorUtils.selectorMatchesElement(
          bindingElement,
          this.cssRule,
          i,
          pseudo,
          true
        )
      ) {
        // Match non :visited selector.
        return false;
      }
    }

    return true;
  }

  /**
   * Return the current node's ancestor that generates its containing block.
   */
  getContainingBlock() {
    return this.node ? InspectorUtils.containingBlockOf(this.node) : null;
  }

  getParentGridElement(node) {
    // The documentElement can't be a grid item, only a container, so bail out.
    if (node.flattenedTreeParentNode === node.ownerDocument) {
      return null;
    }

    if (node.nodeType === node.ELEMENT_NODE) {
      const display = this.style ? this.style.display : null;

      if (!display || display === "none" || display === "contents") {
        // Doesn't generate a box, not a grid item.
        return null;
      }
      if (this.isAbsolutelyPositioned) {
        // Out of flow, not a grid item.
        return null;
      }
    } else if (node.nodeType !== node.TEXT_NODE) {
      return null;
    }

    for (
      let p = node.flattenedTreeParentNode;
      p;
      p = p.flattenedTreeParentNode
    ) {
      if (this.isGridContainer(p)) {
        // It's a grid item!
        return p;
      }

      const style = computedStyle(p, node.ownerGlobal);
      const display = style.display;

      if (display !== "contents") {
        return null; // Not a grid item, for sure.
      }
      // display: contents, walk to the parent
    }
    return null;
  }

  isRowGroup(node) {
    const style = node === this.node ? this.style : computedStyle(node);

    return (
      style &&
      (style.display === "table-row-group" ||
        style.display === "table-header-group" ||
        style.display === "table-footer-group")
    );
  }

  isColumnGroup(node) {
    const style = node === this.node ? this.style : computedStyle(node);

    return style && style.display === "table-column-group";
  }

  /**
   * Check if the given node's writing mode is vertical
   */
  hasVerticalWritingMode(node) {
    const writingMode = computedStyle(node).writingMode;
    return writingMode.includes("vertical") || writingMode.includes("sideways");
  }

  /**
   * Assuming the current element is a table track (row or column) or table track group,
   * get the parent table.
   * This is either going to be the table element if there is one, or the parent element.
   * If the current element is not a table track, this returns the current element.
   *
   * @param  {Boolean} isGroup
   *         Whether the element is a table track group, instead of a table track.
   * @return {DOMNode}
   *         The parent table, the parent element, or the element itself.
   */
  getTableTrackParent(isGroup) {
    let current = this.node.parentNode;

    // Skip over unrendered elements.
    while (computedStyle(current).display === "contents") {
      current = current.parentNode;
    }

    // Skip over groups if the initial element wasn't already one.
    if (!isGroup && (this.isRowGroup(current) || this.isColumnGroup(current))) {
      current = current.parentNode;
    }

    // Once more over unrendered elements above the group.
    while (computedStyle(current).display === "contents") {
      current = current.parentNode;
    }

    return current;
  }
}

/**
 * Returns all CSS property names except given properties.
 *
 * @param {Array} - propertiesToIgnore
 *        Array of property ignored.
 * @return {Array}
 *        Array of all CSS property name except propertiesToIgnore.
 */
function allCssPropertiesExcept(propertiesToIgnore) {
  const properties = new Set(
    InspectorUtils.getCSSPropertyNames({ includeAliases: true })
  );

  for (const name of propertiesToIgnore) {
    properties.delete(name);
  }

  return [...properties];
}

/**
 * Helper for getting an element's computed styles.
 *
 * @param  {DOMNode} node
 *         The node to get the styles for.
 * @param  {Window} window
 *         Optional window object. If omitted, will get the node's window.
 * @return {Object}
 */
function computedStyle(node, window = node.ownerGlobal) {
  return window.getComputedStyle(node);
}

const inactivePropertyHelper = new InactivePropertyHelper();

// The only public method from this module is `isPropertyUsed`.
exports.isPropertyUsed = inactivePropertyHelper.isPropertyUsed.bind(
  inactivePropertyHelper
);
