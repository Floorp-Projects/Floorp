/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { generateUUID } = require("devtools/shared/generate-uuid");
const {
  COMPATIBILITY_TOOLTIP_MESSAGE,
} = require("devtools/client/inspector/rules/constants");

loader.lazyRequireGetter(
  this,
  "escapeCSSComment",
  "devtools/shared/css/parsing-utils",
  true
);

loader.lazyRequireGetter(
  this,
  "getCSSVariables",
  "devtools/client/inspector/rules/utils/utils",
  true
);

/**
 * TextProperty is responsible for the following:
 *   Manages a single property from the authoredText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 */
class TextProperty {
  /**
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
   *        Whether the property is invisible. In an inherited rule, only show
   *        the inherited declarations. The other declarations are considered
   *        invisible and does not show up in the UI. These are needed so that
   *        the index of a property in Rule.textProps is the same as the index
   *        coming from parseDeclarations.
   */
  constructor(rule, name, value, priority, enabled = true, invisible = false) {
    this.id = name + "_" + generateUUID().toString();
    this.rule = rule;
    this.name = name;
    this.value = value;
    this.priority = priority;
    this.enabled = !!enabled;
    this.invisible = invisible;
    this.elementStyle = this.rule.elementStyle;
    this.cssProperties = this.elementStyle.ruleView.cssProperties;
    this.panelDoc = this.elementStyle.ruleView.inspector.panelDoc;
    this.userProperties = this.elementStyle.store.userProperties;
    // Names of CSS variables used in the value of this declaration.
    this.usedVariables = new Set();

    this.updateComputed();
    this.updateUsedVariables();
  }

  get computedProperties() {
    return this.computed
      .filter(computed => computed.name !== this.name)
      .map(computed => {
        return {
          isOverridden: computed.overridden,
          name: computed.name,
          priority: computed.priority,
          value: computed.value,
        };
      });
  }

  /**
   * Returns whether or not the declaration's name is known.
   *
   * @return {Boolean} true if the declaration name is known, false otherwise.
   */
  get isKnownProperty() {
    return this.cssProperties.isKnown(this.name);
  }

  /**
   * Returns whether or not the declaration is changed by the user.
   *
   * @return {Boolean} true if the declaration is changed by the user, false
   * otherwise.
   */
  get isPropertyChanged() {
    return this.userProperties.contains(this.rule.domRule, this.name);
  }

  /**
   * Update the editor associated with this text property,
   * if any.
   */
  updateEditor() {
    // When the editor updates, reset the saved
    // compatibility issues list as any updates
    // may alter the compatibility status of declarations
    this.rule.compatibilityIssues = null;
    if (this.editor) {
      this.editor.update();
    }
  }

  /**
   * Update the list of computed properties for this text property.
   */
  updateComputed() {
    if (!this.name) {
      return;
    }

    // This is a bit funky.  To get the list of computed properties
    // for this text property, we'll set the property on a dummy element
    // and see what the computed style looks like.
    const dummyElement = this.elementStyle.ruleView.dummyElement;
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
  }

  /**
   * Extract all CSS variable names used in this declaration's value into a Set for
   * easy querying. Call this method any time the declaration's value changes.
   */
  updateUsedVariables() {
    this.usedVariables.clear();

    for (const variable of getCSSVariables(this.value)) {
      this.usedVariables.add(variable);
    }
  }

  /**
   * Set all the values from another TextProperty instance into
   * this TextProperty instance.
   *
   * @param {TextProperty} prop
   *        The other TextProperty instance.
   */
  set(prop) {
    let changed = false;
    for (const item of ["name", "value", "priority", "enabled"]) {
      if (this[item] !== prop[item]) {
        this[item] = prop[item];
        changed = true;
      }
    }

    if (changed) {
      this.updateUsedVariables();
      this.updateEditor();
    }
  }

  setValue(value, priority, force = false) {
    if (value !== this.value || force) {
      this.userProperties.setProperty(this.rule.domRule, this.name, value);
    }
    return this.rule.setPropertyValue(this, value, priority).then(() => {
      this.updateUsedVariables();
      this.updateEditor();
    });
  }

  /**
   * Called when the property's value has been updated externally, and
   * the property and editor should update to reflect that value.
   *
   * @param {String} value
   *        Property value
   */
  updateValue(value) {
    if (value !== this.value) {
      this.value = value;
      this.updateUsedVariables();
      this.updateEditor();
    }
  }

  async setName(name) {
    if (name !== this.name) {
      this.userProperties.setProperty(this.rule.domRule, name, this.value);
    }

    await this.rule.setPropertyName(this, name);
    this.updateEditor();
  }

  setEnabled(value) {
    this.rule.setPropertyEnabled(this, value);
    this.updateEditor();
  }

  remove() {
    this.rule.removeProperty(this);
  }

  /**
   * Return a string representation of the rule property.
   */
  stringifyProperty() {
    // Get the displayed property value
    let declaration = this.name + ": " + this.value;

    if (this.priority) {
      declaration += " !" + this.priority;
    }

    declaration += ";";

    // Comment out property declarations that are not enabled
    if (!this.enabled) {
      declaration = "/* " + escapeCSSComment(declaration) + " */";
    }

    return declaration;
  }

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name?
   *
   * @return {Boolean} true if the whole CSS declaration is valid, false otherwise.
   */
  isValid() {
    const selfIndex = this.rule.textProps.indexOf(this);

    // When adding a new property in the rule-view, the TextProperty object is
    // created right away before the rule gets updated on the server, so we're
    // not going to find the corresponding declaration object yet. Default to
    // true.
    if (!this.rule.domRule.declarations[selfIndex]) {
      return true;
    }

    return this.rule.domRule.declarations[selfIndex].isValid;
  }

  isUsed() {
    const selfIndex = this.rule.textProps.indexOf(this);
    const declarations = this.rule.domRule.declarations;

    // StyleRuleActor's declarations may have a isUsed flag (if the server is the right
    // version). Just return true if the information is missing.
    if (
      !declarations ||
      !declarations[selfIndex] ||
      !declarations[selfIndex].isUsed
    ) {
      return { used: true };
    }

    return declarations[selfIndex].isUsed;
  }

  /**
   * Get compatibility issue linked with the textProp.
   *
   * @returns  A JSON objects with compatibility information in following form:
   *    {
   *      // A boolean to denote the compatibility status
   *      isCompatible: <boolean>,
   *      // The CSS declaration that has compatibility issues
   *      property: <string>,
   *      // The un-aliased root CSS declaration for the given property
   *      rootProperty: <string>,
   *      // The l10n message id for the tooltip message
   *      msgId: <string>,
   *      // Link to MDN documentation for the rootProperty
   *      url: <string>,
   *      // An array of all the browsers that don't support the given CSS rule
   *      unsupportedBrowsers: <Array>,
   *    }
   */
  async isCompatible() {
    // This is a workaround for Bug 1648339
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1648339
    // that makes the tooltip icon inconsistent with the
    // position of the rule it is associated with. Once solved,
    // the compatibility data can be directly accessed from the
    // declaration and this logic can be used to set isCompatible
    // property directly to domRule in StyleRuleActor's form() method.
    if (!this.enabled) {
      return { isCompatible: true };
    }

    const compatibilityIssues = await this.rule.getCompatibilityIssues();
    if (!compatibilityIssues.length) {
      return { isCompatible: true };
    }

    const property = this.name;
    const indexOfProperty = compatibilityIssues.findIndex(
      issue => issue.property === property || issue.aliases?.includes(property)
    );

    if (indexOfProperty < 0) {
      return { isCompatible: true };
    }

    const {
      property: rootProperty,
      deprecated,
      experimental,
      url,
      unsupportedBrowsers,
    } = compatibilityIssues[indexOfProperty];

    let msgId = COMPATIBILITY_TOOLTIP_MESSAGE.default;
    if (deprecated && experimental && !unsupportedBrowsers.length) {
      msgId =
        COMPATIBILITY_TOOLTIP_MESSAGE["deprecated-experimental-supported"];
    } else if (deprecated && experimental) {
      msgId = COMPATIBILITY_TOOLTIP_MESSAGE["deprecated-experimental"];
    } else if (deprecated && !unsupportedBrowsers.length) {
      msgId = COMPATIBILITY_TOOLTIP_MESSAGE["deprecated-supported"];
    } else if (deprecated) {
      msgId = COMPATIBILITY_TOOLTIP_MESSAGE.deprecated;
    } else if (experimental && !unsupportedBrowsers.length) {
      msgId = COMPATIBILITY_TOOLTIP_MESSAGE["experimental-supported"];
    } else if (experimental) {
      msgId = COMPATIBILITY_TOOLTIP_MESSAGE.experimental;
    }

    return {
      isCompatible: false,
      property,
      rootProperty,
      msgId,
      url,
      unsupportedBrowsers,
    };
  }

  /**
   * Validate the name of this property.
   *
   * @return {Boolean} true if the property name is valid, false otherwise.
   */
  isNameValid() {
    const selfIndex = this.rule.textProps.indexOf(this);

    // When adding a new property in the rule-view, the TextProperty object is
    // created right away before the rule gets updated on the server, so we're
    // not going to find the corresponding declaration object yet. Default to
    // true.
    if (!this.rule.domRule.declarations[selfIndex]) {
      return true;
    }

    return this.rule.domRule.declarations[selfIndex].isNameValid;
  }

  /**
   * Returns true if the property value is a CSS variables and contains the given variable
   * name, and false otherwise.
   *
   * @param {String}
   *        CSS variable name (e.g. "--color")
   * @return {Boolean}
   */
  hasCSSVariable(name) {
    return this.usedVariables.has(name);
  }
}

module.exports = TextProperty;
