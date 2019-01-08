/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_RULES,
} = require("../actions/index");

const INITIAL_RULES = {
  // Array of CSS rules.
  rules: [],
};

/**
 * Given a rule's TextProperty, returns the properties that are needed to render a
 * CSS declaration.
 *
 * @param  {TextProperty} declaration
 *         A TextProperty of a rule.
 * @param  {Number} index
 *         The index of the CSS declaration within the declaration block.
 * @return {Object} containing the properties needed to render a CSS declaration.
 */
function getDeclarationState(declaration, index) {
  return {
    // Array of the computed properties for a CSS declaration.
    computedProperties: declaration.computedProperties,
    // An unique CSS declaration id.
    id: `${declaration.name}${declaration.value}${index}`,
    // Whether or not the declaration is enabled.
    isEnabled: declaration.enabled,
    // Whether or not the declaration's property name is known.
    isKnownProperty: declaration.isKnownProperty,
    // Whether or not the the declaration is overridden.
    isOverridden: !!declaration.overridden,
    // The declaration's property name.
    name: declaration.name,
    // The declaration's priority (either "important" or an empty string).
    priority: declaration.priority,
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
    declarations: rule.declarations.map((declaration, i) =>
      getDeclarationState(declaration, i)),
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
    // An object containing information about the CSS rule's selector.
    selector: rule.selector,
    // An object containing information about the CSS rule's stylesheet source.
    sourceLink: rule.sourceLink,
    // The CSS rule type.
    type: rule.domRule.type,
  };
}

const reducers = {

  [UPDATE_RULES](_, { rules }) {
    return {
      rules: rules.map(rule => getRuleState(rule)),
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
