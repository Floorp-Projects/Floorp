/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getSourceHash, getRuleHash } = require("../utils/changes-utils");

const {
  RESET_CHANGES,
  TRACK_CHANGE,
} = require("../actions/index");

/**
 * Return a deep clone of the given state object.
 *
 * @param {Object} state
 * @return {Object}
 */
function cloneState(state = {}) {
  return Object.entries(state).reduce((sources, [sourceId, source]) => {
    sources[sourceId] = {
      ...source,
      rules: Object.entries(source.rules).reduce((rules, [ruleId, rule]) => {
        rules[ruleId] = {
          ...rule,
          children: rule.children.slice(0),
          add: { ...rule.add },
          remove: { ...rule.remove },
        };

        return rules;
      }, {}),
    };

    return sources;
  }, {});
}

/**
 * Given information about a CSS rule and its ancestor rules (@media, @supports, etc),
 * create entries in the given rules collection for each rule and assign parent/child
 * dependencies.
 *
 * @param {Object} ruleData
 *        Information about a CSS rule:
 *        {
 *          selector:  {String}
 *                     CSS selector text
 *          ancestors: {Array}
 *                     Flattened CSS rule tree of the rule's ancestors with the root rule
 *                     at the beginning of the array and the leaf rule at the end.
 *          ruleIndex: {Array}
 *                     Indexes of each ancestor rule within its parent rule.
 *        }
 *
 * @param {Object} rules
 *        Collection of rules to be mutated.
 *        This is a reference to the corresponding `rules` object from the state.
 *
 * @return {Object}
 *         Entry for the CSS rule created the given collection of rules.
 */
function createRule(ruleData, rules) {
  // Append the rule data to the flattened CSS rule tree with its ancestors.
  const ruleAncestry = [...ruleData.ancestors, { ...ruleData }];

  return ruleAncestry
    // First, generate a unique identifier for each rule.
    .map((rule, index) => {
      // Ensure each rule has ancestors excluding itself (expand the flattened rule tree).
      rule.ancestors = ruleAncestry.slice(0, index);
      // Ensure each rule has a selector text.
      // For the purpose of displaying in the UI, we treat at-rules as selectors.
      if (!rule.selector) {
        rule.selector =
          `${rule.typeName} ${(rule.conditionText || rule.name || rule.keyText)}`;
      }

      return getRuleHash(rule);
    })
    // Then, create new entries in the rules collection and assign dependencies.
    .map((ruleId, index, array) => {
      const { selector } = ruleAncestry[index];
      const prevRuleId = array[index - 1];
      const nextRuleId = array[index + 1];

      // Copy or create an entry for this rule.
      rules[ruleId] = Object.assign({}, { selector, children: [] }, rules[ruleId]);

      // The next ruleId is lower in the rule tree, therefore it's a child of this rule.
      if (nextRuleId && !rules[ruleId].children.includes(nextRuleId)) {
        rules[ruleId].children.push(nextRuleId);
      }

      // The previous ruleId is higher in the rule tree, therefore it's the parent.
      if (prevRuleId) {
        rules[ruleId].parent = prevRuleId;
      }

      return rules[ruleId];
    })
    // Finally, return the last rule in the array which is the rule we set out to create.
    .pop();
}

function removeRule(ruleId, rules) {
  const rule = rules[ruleId];

  // First, remove this rule's id from its parent's list of children
  if (rule.parent && rules[rule.parent]) {
    rules[rule.parent].children = rules[rule.parent].children.filter(childRuleId => {
      return childRuleId !== ruleId;
    });

    // Remove the parent rule if it has no children left.
    if (!rules[rule.parent].children.length) {
      removeRule(rule.parent, rules);
    }
  }

  delete rules[ruleId];
}

/**
 * Aggregated changes grouped by sources (stylesheet/element), which contain rules,
 * which contain collections of added and removed CSS declarations.
 *
 * Structure:
 *    <sourceId>: {
 *      type: // "stylesheet" or "element"
 *      href: // Stylesheet or document URL
 *      rules: {
 *        <ruleId>: {
 *          selector: "" // String CSS selector or CSS at-rule text
 *          children: [] // Array of <ruleId> for child rules of this rule.
 *          parent:      // <ruleId> of the parent rule
 *          add: {
 *            <property>: <value> // CSS declaration
 *            ...
 *          },
 *          remove: {
 *            <property>: <value> // CSS declaration
 *           ...
 *          }
 *        }
 *        ... // more rules
 *      }
 *    }
 *    ... // more sources
 */
const INITIAL_STATE = {};

const reducers = {

  [TRACK_CHANGE](state, { change }) {
    const defaults = {
      selector: null,
      source: {},
      ancestors: [],
      add: {},
      remove: {},
    };

    change = { ...defaults, ...change };
    state = cloneState(state);

    const { type, href, index } = change.source;
    const { selector, ancestors, ruleIndex } = change;
    const sourceId = getSourceHash(change.source);
    const ruleId = getRuleHash({ selector, ancestors, ruleIndex });

    // Copy or create object identifying the source (styelsheet/element) for this change.
    const source = Object.assign({}, state[sourceId], { type, href, index });
    // Copy or create collection of all rules ever changed in this source.
    const rules = Object.assign({}, source.rules);
    // Refrence or create object identifying the rule for this change.
    let rule = rules[ruleId];
    if (!rule) {
      rule = createRule({ selector, ancestors, ruleIndex }, rules);
    }
    // Copy or create collection of all CSS declarations ever added to this rule.
    const add = Object.assign({}, rule.add);
    // Copy or create collection of all CSS declarations ever removed from this rule.
    const remove = Object.assign({}, rule.remove);

    if (change.remove && change.remove.property) {
      // Track the remove operation only if the property was not previously introduced
      // by an add operation. This ensures repeated changes of the same property
      // register as a single remove operation of its original value.
      if (!add[change.remove.property]) {
        remove[change.remove.property] = change.remove.value;
      }

      // Delete any previous add operation which would be canceled out by this remove.
      if (add[change.remove.property] === change.remove.value) {
        delete add[change.remove.property];
      }
    }

    if (change.add && change.add.property) {
      add[change.add.property] = change.add.value;
    }

    const property = change.add && change.add.property ||
                     change.remove && change.remove.property;

    // Remove tracked operations if they cancel each other out.
    if (add[property] === remove[property]) {
      delete add[property];
      delete remove[property];
    }

    // Remove information about the rule if none its declarations changed.
    if (!Object.keys(add).length && !Object.keys(remove).length) {
      removeRule(ruleId, rules);
      source.rules = { ...rules };
    } else {
      source.rules = { ...rules, [ruleId]: { ...rule, add, remove } };
    }

    // Remove information about the source if none of its rules changed.
    if (!Object.keys(source.rules).length) {
      delete state[sourceId];
    } else {
      state[sourceId] = source;
    }

    return state;
  },

  [RESET_CHANGES](state) {
    return INITIAL_STATE;
  },

};

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return state;
  }
  return reducer(state, action);
};
