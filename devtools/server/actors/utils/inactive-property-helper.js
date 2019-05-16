/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const PREF_UNUSED_CSS_ENABLED = "devtools.inspector.inactive.css.enabled";

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
   *   invalidProperties (see note):
   *     Array of CSS property names that are inactive if the rule matches.
   *   validProperties (see note):
   *     Array of CSS property names that are active if the rule matches.
   *   when:
   *     The rule itself, a JS function used to identify the conditions
   *     indicating whether a property is valid or not.
   *   fixId:
   *     A Fluent id containing a suggested solution to the problem that is
   *     causing a property to be inactive.
   *   msgId:
   *     A Fluent id containing an error message explaining why a property is
   *     inactive in this situation.
   *   numFixProps:
   *     The number of properties we suggest in the fixId string.
   * }
   *
   * NOTE: validProperties and invalidProperties are mutually exclusive.
   *
   * The main export is `isPropertyUsed()`, which can be used to check if a
   * property is used or not, and why.
   */
  get VALIDATORS() {
    return [
      // Flex container property used on non-flex container.
      {
        invalidProperties: [
          "flex-direction",
          "flex-flow",
          "flex-wrap",
        ],
        when: () => !this.flexContainer,
        fixId: "inactive-css-not-flex-container-fix",
        msgId: "inactive-css-not-flex-container",
        numFixProps: 2,
      },
      // Flex item property used on non-flex item.
      {
        invalidProperties: [
          "flex",
          "flex-basis",
          "flex-grow",
          "flex-shrink",
          "order",
        ],
        when: () => !this.flexItem,
        fixId: "inactive-css-not-flex-item-fix",
        msgId: "inactive-css-not-flex-item",
        numFixProps: 2,
      },
      // Grid container property used on non-grid container.
      {
        invalidProperties: [
          "grid-auto-columns",
          "grid-auto-flow",
          "grid-auto-rows",
          "grid-template",
          "grid-gap",
          "row-gap",
          "column-gap",
          "justify-items",
        ],
        when: () => !this.gridContainer,
        fixId: "inactive-css-not-grid-container-fix",
        msgId: "inactive-css-not-grid-container",
        numFixProps: 2,
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
        when: () => !this.gridItem,
        fixId: "inactive-css-not-grid-item-fix",
        msgId: "inactive-css-not-grid-item",
        numFixProps: 2,
      },
      // Grid and flex item properties used on non-grid or non-flex item.
      {
        invalidProperties: [
          "align-self",
        ],
        when: () => !this.gridItem && !this.flexItem,
        fixId: "inactive-css-not-grid-or-flex-item-fix",
        msgId: "inactive-css-not-grid-or-flex-item",
        numFixProps: 4,
      },
      // Grid and flex container properties used on non-grid or non-flex container.
      {
        invalidProperties: [
          "align-content",
          "align-items",
          "justify-content",
        ],
        when: () => !this.gridContainer && !this.flexContainer,
        fixId: "inactive-css-not-grid-or-flex-container-fix",
        msgId: "inactive-css-not-grid-or-flex-container",
        numFixProps: 2,
      },
    ];
  }

  get unusedCssEnabled() {
    if (!this._unusedCssEnabled) {
      this._unusedCssEnabled = Services.prefs.getBoolPref(PREF_UNUSED_CSS_ENABLED, false);
    }
    return this._unusedCssEnabled;
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
   * @return {Boolean} object.fixId
   *         A Fluent id containing a suggested solution to the problem that is
   *         causing a property to be inactive.
   * @return {Boolean} object.msgId
   *         A Fluent id containing an error message explaining why a property
   *         is inactive in this situation.
   * @return {Boolean} object.numFixProps
   *         The number of properties we suggest in the fixId string.
   * @return {Boolean} object.property
   *         The inactive property name.
   * @return {Boolean} object.used
   *         true if the property is used.
   */
  isPropertyUsed(el, elStyle, cssRule, property) {
    if (!this.unusedCssEnabled) {
      return {used: true};
    }

    let fixId = "";
    let msgId = "";
    let numFixProps = 0;
    let used = true;

    this.VALIDATORS.some(validator => {
      // First check if this rule cares about this property.
      let isRuleConcerned = false;

      if (validator.invalidProperties) {
        isRuleConcerned = validator.invalidProperties === "*" ||
                          validator.invalidProperties.includes(property);
      } else if (validator.validProperties) {
        isRuleConcerned = !validator.validProperties.includes(property);
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
        numFixProps = validator.numFixProps;
        used = false;

        return true;
      }

      return false;
    });

    return {
      fixId,
      msgId,
      numFixProps,
      property,
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
   * Check if the current node's propName is set to one of the values passed in
   * the values array.
   *
   * @param {String} propName
   *        Property name to check.
   * @param {Array} values
   *        Values to compare against.
   */
  checkStyle(propName, values) {
    return this.checkStyleForNode(this.node, propName, values);
  }

  /**
   * Check if a node's propName is set to one of the values passed in the values
   * array.
   *
   * @param {DOMNode} node
   *        The node to check.
   * @param {String} propName
   *        Property name to check.
   * @param {Array} values
   *        Values to compare against.
   */
  checkStyleForNode(node, propName, values) {
    return values.some(value => this.style[propName] === value);
  }

  /**
   * Check if the current node is a flex container i.e. a node that has a style
   * of `display:flex` or `display:inline-flex`.
   */
  get flexContainer() {
    return this.checkStyle("display", ["flex", "inline-flex"]);
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
    return this.checkStyle("display", ["grid", "inline-grid"]);
  }

  /**
   * Check if the current node is a grid item.
   */
  get gridItem() {
    return this.isGridItem(this.node);
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
    return !!node.getGridFragments().length > 0;
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

  getParentGridElement(node) {
    if (node.nodeType === node.ELEMENT_NODE) {
      const display = this.style.display;

      if (!display || display === "none" || display === "contents") {
        // Doesn't generate a box, not a grid item.
        return null;
      }
      const position = this.style.position;
      if (position === "absolute" ||
          position === "fixed" ||
          this.style.cssFloat !== "none") {
        // Out of flow, not a grid item.
        return null;
      }
    } else if (node.nodeType !== node.TEXT_NODE) {
      return null;
    }

    for (let p = node.flattenedTreeParentNode; p; p = p.flattenedTreeParentNode) {
      const style = node.ownerGlobal.getComputedStyle(p);
      const display = style.display;

      if (display.includes("grid") && !!p.getGridFragments().length > 0) {
        // It's a grid item!
        return p;
      }
      if (display !== "contents") {
        return null; // Not a grid item, for sure.
      }
      // display: contents, walk to the parent
    }
    return null;
  }
}

exports.inactivePropertyHelper = new InactivePropertyHelper();
