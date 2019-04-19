/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  UPDATE_ADD_RULE_ENABLED,
  UPDATE_HIGHLIGHTED_SELECTOR,
  UPDATE_PRINT_SIMULATION_HIDDEN,
  UPDATE_RULES,
  UPDATE_SOURCE_LINK_ENABLED,
  UPDATE_SOURCE_LINK,
} = require("../actions/index");

const INITIAL_RULES = {
  // The selector of the node that is highlighted by the selector highlighter.
  highlightedSelector: "",
  // Whether or not the add new rule button should be enabled.
  isAddRuleEnabled: false,
  // Whether or not the print simulation button is hidden.
  isPrintSimulationHidden: false,
  // Whether or not the source links are enabled. This is determined by
  // whether or not the style editor is registered.
  isSourceLinkEnabled: Services.prefs.getBoolPref("devtools.styleeditor.enabled"),
  // Array of CSS rules.
  rules: [],
};

/**
 * Given a rule's TextProperty, returns the properties that are needed to render a
 * CSS declaration.
 *
 * @param  {TextProperty} declaration
 *         A TextProperty of a rule.
 * @param  {String} ruleId
 *         The rule id that is associated with the given CSS declaration.
 * @return {Object} containing the properties needed to render a CSS declaration.
 */
function getDeclarationState(declaration, ruleId) {
  return {
    // Array of the computed properties for a CSS declaration.
    computedProperties: declaration.computedProperties,
    // An unique CSS declaration id.
    id: declaration.id,
    // Whether or not the declaration is valid. (Does it make sense for this value
    // to be assigned to this property name?)
    isDeclarationValid: declaration.isValid(),
    // Whether or not the declaration is enabled.
    isEnabled: declaration.enabled,
    // Whether or not the declaration is invisible. In an inherited rule, only the
    // inherited declarations are shown and the rest are considered invisible.
    isInvisible: declaration.invisible,
    // Whether or not the declaration's property name is known.
    isKnownProperty: declaration.isKnownProperty,
    // Whether or not the property name is valid.
    isNameValid: declaration.isNameValid(),
    // Whether or not the the declaration is overridden.
    isOverridden: !!declaration.overridden,
    // Whether or not the declaration is changed by the user.
    isPropertyChanged: declaration.isPropertyChanged,
    // The declaration's property name.
    name: declaration.name,
    // The declaration's priority (either "important" or an empty string).
    priority: declaration.priority,
    // The CSS rule id that is associated with this CSS declaration.
    ruleId,
    // The declaration's property value.
    value: declaration.value,
  };
}

/**
 * Given a Rule, returns the properties that are needed to render a CSS rule.
 *
 * @param  {Rule} rule
 *         A Rule object containing information about a CSS rule.
 * @return {Object} containing the properties needed to render a CSS rule.
 */
function getRuleState(rule) {
  return {
    // Array of CSS declarations.
    declarations: rule.declarations.map(declaration =>
      getDeclarationState(declaration, rule.domRule.actorID)),
    // An unique CSS rule id.
    id: rule.domRule.actorID,
    // An object containing information about the CSS rule's inheritance.
    inheritance: rule.inheritance,
    // Whether or not the rule does not match the current selected element.
    isUnmatched: rule.isUnmatched,
    // Whether or not the rule is an user agent style.
    isUserAgentStyle: rule.isSystem,
    // An object containing information about the CSS keyframes rules.
    keyframesRule: rule.keyframesRule,
    // The pseudo-element keyword used in the rule.
    pseudoElement: rule.pseudoElement,
    // An object containing information about the CSS rule's selector.
    selector: rule.selector,
    // An object containing information about the CSS rule's stylesheet source.
    sourceLink: rule.sourceLink,
    // The CSS rule type.
    type: rule.domRule.type,
  };
}

const reducers = {

  [UPDATE_ADD_RULE_ENABLED](rules, { enabled }) {
    return {
      ...rules,
      isAddRuleEnabled: enabled,
    };
  },

  [UPDATE_HIGHLIGHTED_SELECTOR](rules, { highlightedSelector }) {
    return {
      ...rules,
      highlightedSelector,
    };
  },

  [UPDATE_PRINT_SIMULATION_HIDDEN](rules, { hidden }) {
    return {
      ...rules,
      isPrintSimulationHidden: hidden,
    };
  },

  [UPDATE_RULES](rules, { rules: newRules }) {
    return {
      highlightedSelector: rules.highlightedSelector,
      isAddRuleEnabled: rules.isAddRuleEnabled,
      isPrintSimulationHidden: rules.isPrintSimulationHidden,
      isSourceLinkEnabled: rules.isSourceLinkEnabled,
      rules: newRules.map(rule => getRuleState(rule)),
    };
  },

  [UPDATE_SOURCE_LINK_ENABLED](rules, { enabled }) {
    return {
      ...rules,
      isSourceLinkEnabled: enabled,
    };
  },

  [UPDATE_SOURCE_LINK](rules, { ruleId, sourceLink }) {
    return {
      highlightedSelector: rules.highlightedSelector,
      isAddRuleEnabled: rules.isAddRuleEnabled,
      isSourceLinkEnabled: rules.isSourceLinkEnabled,
      rules: rules.rules.map(rule => {
        if (rule.id !== ruleId) {
          return rule;
        }

        return {
          ...rule,
          sourceLink,
        };
      }),
    };
  },

};

module.exports = function(rules = INITIAL_RULES, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return rules;
  }
  return reducer(rules, action);
};
