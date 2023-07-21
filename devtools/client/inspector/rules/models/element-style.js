/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Rule = require("resource://devtools/client/inspector/rules/models/rule.js");
const UserProperties = require("resource://devtools/client/inspector/rules/models/user-properties.js");
const {
  style: { ELEMENT_STYLE },
} = require("resource://devtools/shared/constants.js");

loader.lazyRequireGetter(
  this,
  "promiseWarn",
  "resource://devtools/client/inspector/shared/utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  ["parseDeclarations", "parseNamedDeclarations", "parseSingleValue"],
  "resource://devtools/shared/css/parsing-utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "isCssVariable",
  "resource://devtools/client/fronts/css-properties.js",
  true
);

const PREF_INACTIVE_CSS_ENABLED = "devtools.inspector.inactive.css.enabled";

/**
 * ElementStyle is responsible for the following:
 *   Keeps track of which properties are overridden.
 *   Maintains a list of Rule objects for a given element.
 */
class ElementStyle {
  /**
   * @param  {Element} element
   *         The element whose style we are viewing.
   * @param  {CssRuleView} ruleView
   *         The instance of the rule-view panel.
   * @param  {Object} store
   *         The ElementStyle can use this object to store metadata
   *         that might outlast the rule view, particularly the current
   *         set of disabled properties.
   * @param  {PageStyleFront} pageStyle
   *         Front for the page style actor that will be providing
   *         the style information.
   * @param  {Boolean} showUserAgentStyles
   *         Should user agent styles be inspected?
   */
  constructor(element, ruleView, store, pageStyle, showUserAgentStyles) {
    this.element = element;
    this.ruleView = ruleView;
    this.store = store || {};
    this.pageStyle = pageStyle;
    this.pseudoElements = [];
    this.showUserAgentStyles = showUserAgentStyles;
    this.rules = [];
    this.cssProperties = this.ruleView.cssProperties;
    this.variablesMap = new Map();

    // We don't want to overwrite this.store.userProperties so we only create it
    // if it doesn't already exist.
    if (!("userProperties" in this.store)) {
      this.store.userProperties = new UserProperties();
    }

    if (!("disabled" in this.store)) {
      this.store.disabled = new WeakMap();
    }
  }

  get unusedCssEnabled() {
    if (!this._unusedCssEnabled) {
      this._unusedCssEnabled = Services.prefs.getBoolPref(
        PREF_INACTIVE_CSS_ENABLED,
        false
      );
    }
    return this._unusedCssEnabled;
  }

  destroy() {
    if (this.destroyed) {
      return;
    }

    this.destroyed = true;
    this.pseudoElements = [];

    for (const rule of this.rules) {
      if (rule.editor) {
        rule.editor.destroy();
      }

      rule.destroy();
    }
  }

  /**
   * Called by the Rule object when it has been changed through the
   * setProperty* methods.
   */
  _changed() {
    if (this.onChanged) {
      this.onChanged();
    }
  }

  /**
   * Refresh the list of rules to be displayed for the active element.
   * Upon completion, this.rules[] will hold a list of Rule objects.
   *
   * Returns a promise that will be resolved when the elementStyle is
   * ready.
   */
  populate() {
    const populated = this.pageStyle
      .getApplied(this.element, {
        inherited: true,
        matchedSelectors: true,
        filter: this.showUserAgentStyles ? "ua" : undefined,
      })
      .then(entries => {
        if (this.destroyed || this.populated !== populated) {
          return Promise.resolve(undefined);
        }

        // Store the current list of rules (if any) during the population
        // process. They will be reused if possible.
        const existingRules = this.rules;

        this.rules = [];

        for (const entry of entries) {
          this._maybeAddRule(entry, existingRules);
        }

        // Store a list of all pseudo-element types found in the matching rules.
        this.pseudoElements = this.rules
          .filter(r => r.pseudoElement)
          .map(r => r.pseudoElement);

        // Mark overridden computed styles.
        this.onRuleUpdated();

        this._sortRulesForPseudoElement();

        // We're done with the previous list of rules.
        for (const r of existingRules) {
          if (r?.editor) {
            r.editor.destroy();
          }

          r.destroy();
        }

        return undefined;
      })
      .catch(e => {
        // populate is often called after a setTimeout,
        // the connection may already be closed.
        if (this.destroyed) {
          return Promise.resolve(undefined);
        }
        return promiseWarn(e);
      });
    this.populated = populated;
    return this.populated;
  }

  /**
   * Returns the Rule object of the given rule id.
   *
   * @param  {String|null} id
   *         The id of the Rule object.
   * @return {Rule|undefined} of the given rule id or undefined if it cannot be found.
   */
  getRule(id) {
    return id
      ? this.rules.find(rule => rule.domRule.actorID === id)
      : undefined;
  }

  /**
   * Get the font families in use by the element.
   *
   * Returns a promise that will be resolved to a list of CSS family
   * names. The list might have duplicates.
   */
  getUsedFontFamilies() {
    return new Promise((resolve, reject) => {
      this.ruleView.styleWindow.requestIdleCallback(async () => {
        if (this.element.isDestroyed()) {
          resolve([]);
          return;
        }
        try {
          const fonts = await this.pageStyle.getUsedFontFaces(this.element, {
            includePreviews: false,
          });
          resolve(fonts.map(font => font.CSSFamilyName));
        } catch (e) {
          reject(e);
        }
      });
    });
  }

  /**
   * Put pseudo elements in front of others.
   */
  _sortRulesForPseudoElement() {
    this.rules = this.rules.sort((a, b) => {
      return (a.pseudoElement || "z") > (b.pseudoElement || "z");
    });
  }

  /**
   * Add a rule if it's one we care about. Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param  {Object} options
   *         Options for creating the Rule, see the Rule constructor.
   * @param  {Array} existingRules
   *         Rules to reuse if possible. If a rule is reused, then it
   *         it will be deleted from this array.
   * @return {Boolean} true if we added the rule.
   */
  _maybeAddRule(options, existingRules) {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (
      options.system ||
      (options.rule && this.rules.some(rule => rule.domRule === options.rule))
    ) {
      return false;
    }

    let rule = null;

    // If we're refreshing and the rule previously existed, reuse the
    // Rule object.
    if (existingRules) {
      const ruleIndex = existingRules.findIndex(r => r.matches(options));
      if (ruleIndex >= 0) {
        rule = existingRules[ruleIndex];
        rule.refresh(options);
        existingRules.splice(ruleIndex, 1);
      }
    }

    // If this is a new rule, create its Rule object.
    if (!rule) {
      rule = new Rule(this, options);
    }

    // Ignore inherited rules with no visible properties.
    if (options.inherited && !rule.hasAnyVisibleProperties()) {
      return false;
    }

    this.rules.push(rule);
    return true;
  }

  /**
   * Calls updateDeclarations with all supported pseudo elements
   */
  onRuleUpdated() {
    this.updateDeclarations();

    // Update declarations for matching rules for pseudo-elements.
    for (const pseudo of this.pseudoElements) {
      this.updateDeclarations(pseudo);
    }
  }

  /**
   * Go over all CSS rules matching the selected element and mark the CSS declarations
   * (aka TextProperty instances) with an `overridden` Boolean flag if an earlier or
   * higher priority declaration overrides it. Rules are already ordered by specificity.
   *
   * If a pseudo-element type is passed (ex: ::before, ::first-line, etc),
   * restrict the operation only to declarations in rules matching that pseudo-element.
   *
   * At the end, update the declaration's view (TextPropertyEditor instance) so it relects
   * the latest state. Use this opportunity to also trigger checks for the "inactive"
   * state of the declaration (whether it has effect or not).
   *
   * @param  {String} pseudo
   *         Optional pseudo-element for which to restrict marking CSS declarations as
   *         overridden.
   */
  updateDeclarations(pseudo = "") {
    // Gather all text properties applicable to the selected element or pseudo-element.
    const textProps = this._getDeclarations(pseudo);
    // Gather all the computed properties applied by those text properties.
    let computedProps = [];
    for (const textProp of textProps) {
      computedProps = computedProps.concat(textProp.computed);
    }

    // CSS Variables inherits from the normal element in case of pseudo element.
    const variables = new Map(pseudo ? this.variablesMap.get("") : null);

    // Walk over the computed properties. As we see a property name
    // for the first time, mark that property's name as taken by this
    // property.
    //
    // If we come across a property whose name is already taken, check
    // its priority against the property that was found first:
    //
    //   If the new property is a higher priority, mark the old
    //   property overridden and mark the property name as taken by
    //   the new property.
    //
    //   If the new property is a lower or equal priority, mark it as
    //   overridden.
    //
    //   Note that this is different if layers are involved: if both
    //   old and new properties have a high priority, and if the new
    //   property is in a rule belonging to a layer that is different
    //   from the the one the old property rule might be in,
    //   mark the old property overridden and mark the property name as
    //   taken by the new property.
    //
    // _overriddenDirty will be set on each prop, indicating whether its
    // dirty status changed during this pass.
    const taken = new Map();
    for (const computedProp of computedProps) {
      const earlier = taken.get(computedProp.name);

      // Prevent -webkit-gradient from being selected after unchecking
      // linear-gradient in this case:
      //  -moz-linear-gradient: ...;
      //  -webkit-linear-gradient: ...;
      //  linear-gradient: ...;
      if (!computedProp.textProp.isValid()) {
        computedProp.overridden = true;
        continue;
      }

      let overridden;
      if (
        earlier &&
        computedProp.priority === "important" &&
        (earlier.priority !== "important" ||
          // Even if the earlier property was important, if the current rule is in a layer
          // it will take precedence, unless the earlier property rule was in the same layer.
          (computedProp.textProp.rule?.isInLayer() &&
            computedProp.textProp.rule.isInDifferentLayer(
              earlier.textProp.rule
            ))) &&
        // For !important only consider rules applying to the same parent node.
        computedProp.textProp.rule.inherited == earlier.textProp.rule.inherited
      ) {
        // New property is higher priority. Mark the earlier property
        // overridden (which will reverse its dirty state).
        earlier._overriddenDirty = !earlier._overriddenDirty;
        earlier.overridden = true;
        overridden = false;
      } else {
        overridden = !!earlier;
      }

      computedProp._overriddenDirty = !!computedProp.overridden !== overridden;
      computedProp.overridden = overridden;

      if (!computedProp.overridden && computedProp.textProp.enabled) {
        taken.set(computedProp.name, computedProp);

        if (isCssVariable(computedProp.name)) {
          variables.set(computedProp.name, computedProp.value);
        }
      }
    }

    // Find the CSS variables that have been updated.
    const previousVariablesMap = new Map(this.variablesMap.get(pseudo));
    const changedVariableNamesSet = new Set(
      [...variables.keys(), ...previousVariablesMap.keys()].filter(
        k => variables.get(k) !== previousVariablesMap.get(k)
      )
    );

    this.variablesMap.set(pseudo, variables);

    // For each TextProperty, mark it overridden if all of its computed
    // properties are marked overridden. Update the text property's associated
    // editor, if any. This will clear the _overriddenDirty state on all
    // computed properties. For each editor we also show or hide the inactive
    // CSS icon as needed.
    for (const textProp of textProps) {
      // _updatePropertyOverridden will return true if the
      // overridden state has changed for the text property.
      // _hasUpdatedCSSVariable will return true if the declaration contains any
      // of the updated CSS variable names.
      if (
        this._updatePropertyOverridden(textProp) ||
        this._hasUpdatedCSSVariable(textProp, changedVariableNamesSet)
      ) {
        textProp.updateEditor();
      }

      // For each editor show or hide the inactive CSS icon as needed.
      if (textProp.editor && this.unusedCssEnabled) {
        textProp.editor.updatePropertyState();
      }
    }
  }

  /**
   * Returns true if the given declaration's property value contains a CSS variable
   * matching any of the updated CSS variable names.
   *
   * @param {TextProperty} declaration
   *        A TextProperty of a rule.
   * @param {Set<>String} variableNamesSet
   *        A Set of CSS variable names that have been updated.
   */
  _hasUpdatedCSSVariable(declaration, variableNamesSet) {
    for (const variableName of variableNamesSet) {
      if (declaration.hasCSSVariable(variableName)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Helper for |this.updateDeclarations()| to mark CSS declarations as overridden.
   *
   * Returns an array of CSS declarations (aka TextProperty instances) from all rules
   * applicable to the selected element ordered from more- to less-specific.
   *
   * If a pseudo-element type is given, restrict the result only to declarations
   * applicable to that pseudo-element.
   *
   * NOTE: this method skips CSS declarations in @keyframes rules because a number of
   * criteria such as time and animation delay need to be checked in order to determine
   * if the property is overridden at runtime.
   *
   * @param  {String} pseudo
   *         Optional pseudo-element for which to restrict marking CSS declarations as
   *         overridden. If omitted, only declarations for regular style rules are
   *         returned (no pseudo-element style rules).
   *
   * @return {Array}
   *         Array of TextProperty instances.
   */
  _getDeclarations(pseudo = "") {
    const textProps = [];

    for (const rule of this.rules) {
      // Skip @keyframes rules
      if (rule.keyframes) {
        continue;
      }

      // Style rules must be considered only when they have selectors that match the node.
      // When renaming a selector, the unmatched rule lingers in the Rule view, but it no
      // longer matches the node. This strict check avoids accidentally causing
      // declarations to be overridden in the remaining matching rules.
      const isStyleRule =
        rule.pseudoElement === "" && !!rule.matchedDesugaredSelectors.length;

      // Style rules for pseudo-elements must always be considered, regardless if their
      // selector matches the node. As a convenience, declarations in rules for
      // pseudo-elements show up in a separate Pseudo-elements accordion when selecting
      // the host node (instead of the pseudo-element node directly, which is sometimes
      // impossible, for example with ::selection or ::first-line).
      // Loosening the strict check on matched selectors ensures these declarations
      // participate in the algorithm below to mark them as overridden.
      const isPseudoElementRule =
        rule.pseudoElement !== "" && rule.pseudoElement === pseudo;

      const isElementStyle = rule.domRule.type === ELEMENT_STYLE;

      const filterCondition =
        pseudo === "" ? isStyleRule || isElementStyle : isPseudoElementRule;

      // Collect all relevant CSS declarations (aka TextProperty instances).
      if (filterCondition) {
        for (const textProp of rule.textProps.slice(0).reverse()) {
          if (textProp.enabled) {
            textProps.push(textProp);
          }
        }
      }
    }

    return textProps;
  }

  /**
   * Adds a new declaration to the rule.
   *
   * @param  {String} ruleId
   *         The id of the Rule to be modified.
   * @param  {String} value
   *         The new declaration value.
   */
  addNewDeclaration(ruleId, value) {
    const rule = this.getRule(ruleId);
    if (!rule) {
      return;
    }

    const declarationsToAdd = parseNamedDeclarations(
      this.cssProperties.isKnown,
      value,
      true
    );
    if (!declarationsToAdd.length) {
      return;
    }

    this._addMultipleDeclarations(rule, declarationsToAdd);
  }

  /**
   * Adds a new rule. The rules view is updated from a "stylesheet-updated" event
   * emitted the PageStyleActor as a result of the rule being inserted into the
   * the stylesheet.
   */
  async addNewRule() {
    await this.pageStyle.addNewRule(
      this.element,
      this.element.pseudoClassLocks
    );
  }

  /**
   * Given the id of the rule and the new declaration name, modifies the existing
   * declaration name to the new given value.
   *
   * @param  {String} ruleId
   *         The Rule id of the given CSS declaration.
   * @param  {String} declarationId
   *         The TextProperty id for the CSS declaration.
   * @param  {String} name
   *         The new declaration name.
   */
  async modifyDeclarationName(ruleId, declarationId, name) {
    const rule = this.getRule(ruleId);
    if (!rule) {
      return;
    }

    const declaration = rule.getDeclaration(declarationId);
    if (!declaration || declaration.name === name) {
      return;
    }

    // Adding multiple rules inside of name field overwrites the current
    // property with the first, then adds any more onto the property list.
    const declarations = parseDeclarations(this.cssProperties.isKnown, name);
    if (!declarations.length) {
      return;
    }

    await declaration.setName(declarations[0].name);

    if (!declaration.enabled) {
      await declaration.setEnabled(true);
    }
  }

  /**
   * Helper function to addNewDeclaration() and modifyDeclarationValue() for
   * adding multiple declarations to a rule.
   *
   * @param  {Rule} rule
   *         The Rule object to write new declarations to.
   * @param  {Array<Object>} declarationsToAdd
   *         An array of object containg the parsed declaration data to be added.
   * @param  {TextProperty|null} siblingDeclaration
   *         Optional declaration next to which the new declaration will be added.
   */
  _addMultipleDeclarations(rule, declarationsToAdd, siblingDeclaration = null) {
    for (const { commentOffsets, name, value, priority } of declarationsToAdd) {
      const isCommented = Boolean(commentOffsets);
      const enabled = !isCommented;
      siblingDeclaration = rule.createProperty(
        name,
        value,
        priority,
        enabled,
        siblingDeclaration
      );
    }
  }

  /**
   * Parse a value string and break it into pieces, starting with the
   * first value, and into an array of additional declarations (if any).
   *
   * Example: Calling with "red; width: 100px" would return
   * { firstValue: "red", propertiesToAdd: [{ name: "width", value: "100px" }] }
   *
   * @param  {String} value
   *         The string to parse.
   * @return {Object} An object with the following properties:
   *         firstValue: A string containing a simple value, like
   *                     "red" or "100px!important"
   *         declarationsToAdd: An array with additional declarations, following the
   *                            parseDeclarations format of { name, value, priority }
   */
  _getValueAndExtraProperties(value) {
    // The inplace editor will prevent manual typing of multiple declarations,
    // but we need to deal with the case during a paste event.
    // Adding multiple declarations inside of value editor sets value with the
    // first, then adds any more onto the declaration list (below this declarations).
    let firstValue = value;
    let declarationsToAdd = [];

    const declarations = parseDeclarations(this.cssProperties.isKnown, value);

    // Check to see if the input string can be parsed as multiple declarations
    if (declarations.length) {
      // Get the first property value (if any), and any remaining
      // declarations (if any)
      if (!declarations[0].name && declarations[0].value) {
        firstValue = declarations[0].value;
        declarationsToAdd = declarations.slice(1);
      } else if (declarations[0].name && declarations[0].value) {
        // In some cases, the value could be a property:value pair
        // itself.  Join them as one value string and append
        // potentially following declarations
        firstValue = declarations[0].name + ": " + declarations[0].value;
        declarationsToAdd = declarations.slice(1);
      }
    }

    return {
      declarationsToAdd,
      firstValue,
    };
  }

  /**
   * Given the id of the rule and the new declaration value, modifies the existing
   * declaration value to the new given value.
   *
   * @param  {String} ruleId
   *         The Rule id of the given CSS declaration.
   * @param  {String} declarationId
   *         The TextProperty id for the CSS declaration.
   * @param  {String} value
   *         The new declaration value.
   */
  async modifyDeclarationValue(ruleId, declarationId, value) {
    const rule = this.getRule(ruleId);
    if (!rule) {
      return;
    }

    const declaration = rule.getDeclaration(declarationId);
    if (!declaration) {
      return;
    }

    const { declarationsToAdd, firstValue } =
      this._getValueAndExtraProperties(value);
    const parsedValue = parseSingleValue(
      this.cssProperties.isKnown,
      firstValue
    );

    if (
      !declarationsToAdd.length &&
      declaration.value === parsedValue.value &&
      declaration.priority === parsedValue.priority
    ) {
      return;
    }

    // First, set this declaration value (common case, only modified a property)
    await declaration.setValue(parsedValue.value, parsedValue.priority);

    if (!declaration.enabled) {
      await declaration.setEnabled(true);
    }

    this._addMultipleDeclarations(rule, declarationsToAdd, declaration);
  }

  /**
   * Modifies the existing rule's selector to the new given value.
   *
   * @param  {String} ruleId
   *         The id of the Rule to be modified.
   * @param  {String} selector
   *         The new selector value.
   */
  async modifySelector(ruleId, selector) {
    try {
      const rule = this.getRule(ruleId);
      if (!rule) {
        return;
      }

      const response = await rule.domRule.modifySelector(
        this.element,
        selector
      );
      const { ruleProps, isMatching } = response;

      if (!ruleProps) {
        // Notify for changes, even when nothing changes, just to allow tests
        // being able to track end of this request.
        this.ruleView.emit("ruleview-invalid-selector");
        return;
      }

      const newRule = new Rule(this, {
        ...ruleProps,
        isUnmatched: !isMatching,
      });

      // Recompute the list of applied styles because editing a
      // selector might cause this rule's position to change.
      const appliedStyles = await this.pageStyle.getApplied(this.element, {
        inherited: true,
        matchedSelectors: true,
        filter: this.showUserAgentStyles ? "ua" : undefined,
      });
      const newIndex = appliedStyles.findIndex(r => r.rule == ruleProps.rule);
      const oldIndex = this.rules.indexOf(rule);

      // Remove the old rule and insert the new rule according to where it appears
      // in the list of applied styles.
      this.rules.splice(oldIndex, 1);
      // If the selector no longer matches, then we leave the rule in
      // the same relative position.
      this.rules.splice(newIndex === -1 ? oldIndex : newIndex, 0, newRule);

      // Recompute, mark and update the UI for any properties that are
      // overridden or contain inactive CSS according to the new list of rules.
      this.onRuleUpdated();

      // In order to keep the new rule in place of the old in the rules view, we need
      // to remove the rule again if the rule was inserted to its new index according
      // to the list of applied styles.
      // Note: you might think we would replicate the list-modification logic above,
      // but that is complicated due to the way the UI installs pseudo-element rules
      // and the like.
      if (newIndex !== -1) {
        this.rules.splice(newIndex, 1);
        this.rules.splice(oldIndex, 0, newRule);
      }
      this._changed();
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Toggles the enabled state of the given CSS declaration.
   *
   * @param  {String} ruleId
   *         The Rule id of the given CSS declaration.
   * @param  {String} declarationId
   *         The TextProperty id for the CSS declaration.
   */
  toggleDeclaration(ruleId, declarationId) {
    const rule = this.getRule(ruleId);
    if (!rule) {
      return;
    }

    const declaration = rule.getDeclaration(declarationId);
    if (!declaration) {
      return;
    }

    declaration.setEnabled(!declaration.enabled);
  }

  /**
   * Mark a given TextProperty as overridden or not depending on the
   * state of its computed properties. Clears the _overriddenDirty state
   * on all computed properties.
   *
   * @param  {TextProperty} prop
   *         The text property to update.
   * @return {Boolean} true if the TextProperty's overridden state (or any of
   *         its computed properties overridden state) changed.
   */
  _updatePropertyOverridden(prop) {
    let overridden = true;
    let dirty = false;

    for (const computedProp of prop.computed) {
      if (!computedProp.overridden) {
        overridden = false;
      }

      dirty = computedProp._overriddenDirty || dirty;
      delete computedProp._overriddenDirty;
    }

    dirty = !!prop.overridden !== overridden || dirty;
    prop.overridden = overridden;
    return dirty;
  }

  /**
   * Returns the current value of a CSS variable; or null if the
   * variable is not defined.
   *
   * @param  {String} name
   *         The name of the variable.
   * @param  {String} pseudo
   *         The pseudo-element name of the rule.
   * @return {String} the variable's value or null if the variable is
   *         not defined.
   */
  getVariable(name, pseudo = "") {
    const variables = this.variablesMap.get(pseudo);
    return variables ? variables.get(name) : null;
  }
}

module.exports = ElementStyle;
