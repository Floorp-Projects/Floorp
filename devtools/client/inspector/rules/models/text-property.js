/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {escapeCSSComment} = require("devtools/shared/css/parsing-utils");
const {getCssProperties} = require("devtools/shared/fronts/css-properties");

/**
 * TextProperty is responsible for the following:
 *   Manages a single property from the authoredText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 *
 * @param {Rule} rule
 *        The rule this TextProperty came from.
 * @param {String} name
 *        The text property name (such as "background" or "border-top").
 * @param {String} value
 *        The property's value (not including priority).
 * @param {String} priority
 *        The property's priority (either "important" or an empty string).
 * @param {Boolean} enabled
 *        Whether the property is enabled.
 * @param {Boolean} invisible
 *        Whether the property is invisible.  An invisible property
 *        does not show up in the UI; these are needed so that the
 *        index of a property in Rule.textProps is the same as the index
 *        coming from parseDeclarations.
 */
function TextProperty(rule, name, value, priority, enabled = true,
                      invisible = false) {
  this.rule = rule;
  this.name = name;
  this.value = value;
  this.priority = priority;
  this.enabled = !!enabled;
  this.invisible = invisible;
  this.panelDoc = this.rule.elementStyle.ruleView.inspector.panelDoc;

  const toolbox = this.rule.elementStyle.ruleView.inspector.toolbox;
  this.cssProperties = getCssProperties(toolbox);

  this.updateComputed();
}

TextProperty.prototype = {
  /**
   * Update the editor associated with this text property,
   * if any.
   */
  updateEditor: function() {
    if (this.editor) {
      this.editor.update();
    }
  },

  /**
   * Update the list of computed properties for this text property.
   */
  updateComputed: function() {
    if (!this.name) {
      return;
    }

    // This is a bit funky.  To get the list of computed properties
    // for this text property, we'll set the property on a dummy element
    // and see what the computed style looks like.
    const dummyElement = this.rule.elementStyle.ruleView.dummyElement;
    const dummyStyle = dummyElement.style;
    dummyStyle.cssText = "";
    dummyStyle.setProperty(this.name, this.value, this.priority);

    this.computed = [];

    // Manually get all the properties that are set when setting a value on
    // this.name and check the computed style on dummyElement for each one.
    // If we just read dummyStyle, it would skip properties when value === "".
    const subProps = this.cssProperties.getSubproperties(this.name);

    for (const prop of subProps) {
      this.computed.push({
        textProp: this,
        name: prop,
        value: dummyStyle.getPropertyValue(prop),
        priority: dummyStyle.getPropertyPriority(prop),
      });
    }
  },

  /**
   * Set all the values from another TextProperty instance into
   * this TextProperty instance.
   *
   * @param {TextProperty} prop
   *        The other TextProperty instance.
   */
  set: function(prop) {
    let changed = false;
    for (const item of ["name", "value", "priority", "enabled"]) {
      if (this[item] !== prop[item]) {
        this[item] = prop[item];
        changed = true;
      }
    }

    if (changed) {
      this.updateEditor();
    }
  },

  setValue: function(value, priority, force = false) {
    const store = this.rule.elementStyle.store;

    if (this.editor && value !== this.editor.committed.value || force) {
      store.userProperties.setProperty(this.rule.style, this.name, value);
    }

    this.rule.setPropertyValue(this, value, priority);
    this.updateEditor();
  },

  /**
   * Called when the property's value has been updated externally, and
   * the property and editor should update to reflect that value.
   *
   * @param {String} value
   *        Property value
   */
  updateValue: function(value) {
    if (value !== this.value) {
      this.value = value;
      this.updateEditor();
    }
  },

  setName: function(name) {
    const store = this.rule.elementStyle.store;

    if (name !== this.name) {
      store.userProperties.setProperty(this.rule.style, name,
                                       this.editor.committed.value);
    }

    this.rule.setPropertyName(this, name);
    this.updateEditor();
  },

  setEnabled: function(value) {
    this.rule.setPropertyEnabled(this, value);
    this.updateEditor();
  },

  remove: function() {
    this.rule.removeProperty(this);
  },

  /**
   * Return a string representation of the rule property.
   */
  stringifyProperty: function() {
    // Get the displayed property value
    let declaration = this.name + ": " + this.editor.valueSpan.textContent +
      ";";

    // Comment out property declarations that are not enabled
    if (!this.enabled) {
      declaration = "/* " + escapeCSSComment(declaration) + " */";
    }

    return declaration;
  },

  /**
   * See whether this property's name is known.
   *
   * @return {Boolean} true if the property name is known, false otherwise.
   */
  isKnownProperty: function() {
    return this.cssProperties.isKnown(this.name);
  },

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name?
   *
   * @return {Boolean} true if the whole CSS declaration is valid, false otherwise.
   */
  isValid: function() {
    const selfIndex = this.rule.textProps.indexOf(this);

    // When adding a new property in the rule-view, the TextProperty object is
    // created right away before the rule gets updated on the server, so we're
    // not going to find the corresponding declaration object yet. Default to
    // true.
    if (!this.rule.domRule.declarations[selfIndex]) {
      return true;
    }

    return this.rule.domRule.declarations[selfIndex].isValid;
  },

  /**
   * Validate the name of this property.
   *
   * @return {Boolean} true if the property name is valid, false otherwise.
   */
  isNameValid: function() {
    const selfIndex = this.rule.textProps.indexOf(this);

    // When adding a new property in the rule-view, the TextProperty object is
    // created right away before the rule gets updated on the server, so we're
    // not going to find the corresponding declaration object yet. Default to
    // true.
    if (!this.rule.domRule.declarations[selfIndex]) {
      return true;
    }

    // Starting with FF61, StyleRuleActor provides an accessor to signal if the property
    // name is valid. If we don't have this, assume the name is valid. In use, rely on
    // isValid() as a guard against false positives.
    return (this.rule.domRule.declarations[selfIndex].isNameValid !== undefined)
      ? this.rule.domRule.declarations[selfIndex].isNameValid
      : true;
  }
};

module.exports = TextProperty;
