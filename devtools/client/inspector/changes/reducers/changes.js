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
          add: rule.add.slice(0),
          remove: rule.remove.slice(0),
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
      const defaults = { selector, ruleId, add: [], remove: [], children: [] };
      rules[ruleId] = Object.assign(defaults, rules[ruleId]);

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
 *      type: // {String} One of: "stylesheet", "inline" or "element"
 *      href: // {String|null} Stylesheet or document URL; null for inline stylesheets
 *      rules: {
 *        <ruleId>: {
 *          ruleId:      // {String} <ruleId> of this rule
 *          selector:    // {String} CSS selector or CSS at-rule text
 *          changeType:  // {String} Optional; one of: "rule-add" or "rule-remove"
 *          children: [] // {Array} of <ruleId> for child rules of this rule
 *          parent:      // {String} <ruleId> of the parent rule
 *          add: [       // {Array} of objects with CSS declarations
 *            {
 *              property:    // {String} CSS property name
 *              value:       // {String} CSS property value
 *              index:       // {Number} Position of the declaration within its CSS rule
 *            }
 *            ... // more declarations
 *          ],
 *          remove: []   // {Array} of objects with CSS declarations
 *        }
 *        ... // more rules
 *      }
 *    }
 *    ... // more sources
 */
const INITIAL_STATE = {};

const reducers = {

  /**
   * CSS changes are collected on the server by the ChangesActor which dispatches them to
   * the client as atomic operations: a rule/declaration updated, added or removed.
   *
   * By design, the ChangesActor has no big-picture context of all the collected changes.
   * It only holds the stack of atomic changes. This makes it roboust for many use cases:
   * building a diff-view, supporting undo/redo, offline persistence, etc. Consumers,
   * like the Changes panel, get to massage the data for their particular purposes.
   *
   * Here in the reducer, we aggregate incoming changes to build a human-readable diff
   * shown in the Changes panel.
   * - added / removed declarations are grouped by CSS rule. Rules are grouped by their
   *   parent rules (@media, @supports, @keyframes, etc.); Rules belong to sources
   *   (stylesheets, inline styles)
   * - declarations have an index corresponding to their position in the CSS rule. This
   *   allows tracking of multiple declarations with the same property name.
   * - repeated changes a declaration will show only the original removal and the latest
   *   addition;
   * - when a declaration is removed, we update the indices of other tracked declarations
   *   in the same rule which may have changed position in the rule as a result;
   * - changes which cancel each other out (i.e. return to original) are both removed
   *   from the store;
   * - when changes cancel each other out leaving the rule unchanged, the rule is removed
   *   from the store. Its parent rule is removed as well if it too ends up unchanged.
   */
  [TRACK_CHANGE](state, { change }) {
    const defaults = {
      selector: null,
      source: {},
      ancestors: [],
      add: [],
      remove: [],
    };

    change = { ...defaults, ...change };
    state = cloneState(state);

    const { type, href, index, isFramed } = change.source;
    const { selector, ancestors, ruleIndex, type: changeType } = change;
    const sourceId = getSourceHash(change.source);
    const ruleId = getRuleHash({ selector, ancestors, ruleIndex });

    // Copy or create object identifying the source (styelsheet/element) for this change.
    const source = Object.assign({}, state[sourceId], { type, href, index, isFramed });
    // Copy or create collection of all rules ever changed in this source.
    const rules = Object.assign({}, source.rules);
    // Refrence or create object identifying the rule for this change.
    let rule = rules[ruleId];
    if (!rule) {
      rule = createRule({ selector, ancestors, ruleIndex }, rules);
      if (changeType.startsWith("rule-")) {
        rule.changeType = changeType;
      }
    }

    if (change.remove && change.remove.length) {
      for (const decl of change.remove) {
        // Find the position of any added declaration which matches the incoming
        // declaration to be removed.
        const addIndex = rule.add.findIndex(addDecl => {
          return addDecl.index === decl.index &&
                 addDecl.property === decl.property &&
                 addDecl.value === decl.value;
        });

        // Find the position of any removed declaration which matches the incoming
        // declaration to be removed. It's possible to get duplicate remove operations
        // when, for example, disabling a declaration then deleting it.
        const removeIndex = rule.remove.findIndex(removeDecl => {
          return removeDecl.index === decl.index &&
                 removeDecl.property === decl.property &&
                 removeDecl.value === decl.value;
        });

        // Track the remove operation only if the property was not previously introduced
        // by an add operation. This ensures repeated changes of the same property
        // register as a single remove operation of its original value. Avoid tracking the
        // remove declaration if already tracked (happens on disable followed by delete).
        if (addIndex < 0 && removeIndex < 0) {
          rule.remove.push(decl);
        }

        // Delete any previous add operation which would be canceled out by this remove.
        if (rule.add[addIndex]) {
          rule.add.splice(addIndex, 1);
        }

        // Update the indexes of previously tracked declarations which follow this removed
        // one so future tracking continues to point to the right declarations.
        if (changeType === "declaration-remove") {
          rule.add = rule.add.map((addDecl => {
            if (addDecl.index > decl.index) {
              addDecl.index--;
            }

            return addDecl;
          }));

          rule.remove = rule.remove.map((removeDecl => {
            if (removeDecl.index > decl.index) {
              removeDecl.index--;
            }

            return removeDecl;
          }));
        }
      }
    }

    if (change.add && change.add.length) {
      for (const decl of change.add) {
        // Find the position of any removed declaration which matches the incoming
        // declaration to be added.
        const removeIndex = rule.remove.findIndex(removeDecl => {
          return removeDecl.index === decl.index &&
                 removeDecl.value === decl.value &&
                 removeDecl.property === decl.property;
        });

        // Find the position of any added declaration which matches the incoming
        // declaration to be added in case we need to replace it.
        const addIndex = rule.add.findIndex(addDecl => {
          return addDecl.index === decl.index &&
                 addDecl.property === decl.property;
        });

        if (rule.remove[removeIndex]) {
          // Delete any previous remove operation which would be canceled out by this add.
          rule.remove.splice(removeIndex, 1);
        } else if (rule.add[addIndex]) {
          // Replace previous add operation for declaration at this index.
          rule.add.splice(addIndex, 1, decl);
        } else {
          // Track new add operation.
          rule.add.push(decl);
        }
      }
    }

    // Remove information about the rule if none its declarations changed.
    if (!rule.add.length && !rule.remove.length) {
      removeRule(ruleId, rules);
      source.rules = { ...rules };
    } else {
      source.rules = { ...rules, [ruleId]: rule };
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
