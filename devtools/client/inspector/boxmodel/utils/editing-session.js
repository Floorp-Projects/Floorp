/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * An instance of EditingSession tracks changes that have been made during the
 * modification of box model values. All of these changes can be reverted by
 * calling revert.
 *
 * @param  {InspectorPanel} inspector
 *         The inspector panel.
 * @param  {Document} doc
 *         A DOM document that can be used to test style rules.
 * @param  {Array} rules
 *         An array of the style rules defined for the node being
 *         edited. These should be in order of priority, least
 *         important first.
 */
function EditingSession({ inspector, doc, elementRules }) {
  this._doc = doc;
  this._inspector = inspector;
  this._rules = elementRules;
  this._modifications = new Map();
}

EditingSession.prototype = {
  /**
   * Gets the value of a single property from the CSS rule.
   *
   * @param  {StyleRuleFront} rule
   *         The CSS rule.
   * @param  {String} property
   *         The name of the property.
   * @return {String} the value.
   */
  getPropertyFromRule(rule, property) {
    // Use the parsed declarations in the StyleRuleFront object if available.
    const index = this.getPropertyIndex(property, rule);
    if (index !== -1) {
      return rule.declarations[index].value;
    }

    // Fallback to parsing the cssText locally otherwise.
    const dummyStyle = this._element.style;
    dummyStyle.cssText = rule.cssText;
    return dummyStyle.getPropertyValue(property);
  },

  /**
   * Returns the current value for a property as a string or the empty string if
   * no style rules affect the property.
   *
   * @param  {String} property
   *         The name of the property as a string
   */
  getProperty(property) {
    // Create a hidden element for getPropertyFromRule to use
    const div = this._doc.createElement("div");
    div.setAttribute("style", "display: none");
    this._doc.getElementById("inspector-main-content").appendChild(div);
    this._element = this._doc.createElement("p");
    div.appendChild(this._element);

    // As the rules are in order of priority we can just iterate until we find
    // the first that defines a value for the property and return that.
    for (const rule of this._rules) {
      const value = this.getPropertyFromRule(rule, property);
      if (value !== "") {
        div.remove();
        return value;
      }
    }
    div.remove();
    return "";
  },

  /**
   * Get the index of a given css property name in a CSS rule.
   * Or -1, if there are no properties in the rule yet.
   *
   * @param  {String} name
   *         The property name.
   * @param  {StyleRuleFront} rule
   *         Optional, defaults to the element style rule.
   * @return {Number} The property index in the rule.
   */
  getPropertyIndex(name, rule = this._rules[0]) {
    if (!rule.declarations.length) {
      return -1;
    }

    return rule.declarations.findIndex(p => p.name === name);
  },

  /**
   * Sets a number of properties on the node.
   *
   * @param  {Array} properties
   *         An array of properties, each is an object with name and
   *         value properties. If the value is "" then the property
   *         is removed.
   * @return {Promise} Resolves when the modifications are complete.
   */
  async setProperties(properties) {
    for (const property of properties) {
      // Get a RuleModificationList or RuleRewriter helper object from the
      // StyleRuleActor to make changes to CSS properties.
      // Note that RuleRewriter doesn't support modifying several properties at
      // once, so we do this in a sequence here.
      const modifications = this._rules[0].startModifyingProperties(
        this._inspector.cssProperties
      );

      // Remember the property so it can be reverted.
      if (!this._modifications.has(property.name)) {
        this._modifications.set(
          property.name,
          this.getPropertyFromRule(this._rules[0], property.name)
        );
      }

      // Find the index of the property to be changed, or get the next index to
      // insert the new property at.
      let index = this.getPropertyIndex(property.name);
      if (index === -1) {
        index = this._rules[0].declarations.length;
      }

      if (property.value == "") {
        modifications.removeProperty(index, property.name);
      } else {
        modifications.setProperty(index, property.name, property.value, "");
      }

      await modifications.apply();
    }
  },

  /**
   * Reverts all of the property changes made by this instance.
   *
   * @return {Promise} Resolves when all properties have been reverted.
   */
  async revert() {
    // Revert each property that we modified previously, one by one. See
    // setProperties for information about why.
    for (const [property, value] of this._modifications) {
      const modifications = this._rules[0].startModifyingProperties(
        this._inspector.cssProperties
      );

      // Find the index of the property to be reverted.
      let index = this.getPropertyIndex(property);

      if (value != "") {
        // If the property doesn't exist anymore, insert at the beginning of the
        // rule.
        if (index === -1) {
          index = 0;
        }
        modifications.setProperty(index, property, value, "");
      } else {
        // If the property doesn't exist anymore, no need to remove it. It had
        // not been added after all.
        if (index === -1) {
          continue;
        }
        modifications.removeProperty(index, property);
      }

      await modifications.apply();
    }
  },

  destroy() {
    this._modifications.clear();

    this._cssProperties = null;
    this._doc = null;
    this._inspector = null;
    this._modifications = null;
    this._rules = null;
  },
};

module.exports = EditingSession;
