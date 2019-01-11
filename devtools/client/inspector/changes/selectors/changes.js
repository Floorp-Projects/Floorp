/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * In the Redux state, changed CSS rules are grouped by source (stylesheet) and stored in
 * a single level array, regardless of nesting.
 * This method returns a nested tree structure of the changed CSS rules so the React
 * consumer components can traverse it easier when rendering the nested CSS rules view.
 * Keeping this interface updated allows the Redux state structure to change without
 * affecting the consumer components.
 *
 * @param {Object} state
 *        Redux slice for tracked changes.
 * @return {Object}
 */
function getChangesTree(state) {
  /**
   * Recursively replace a rule's array of child rule ids with the referenced child rules.
   * Mark visited rules so as not to handle them (and their children) again.
   *
   * Returns the rule object with expanded children or null if previously visited.
   *
   * @param  {String} ruleId
   * @param  {Object} rule
   * @param  {Array} rules
   * @param  {Set} visitedRules
   * @return {Object|null}
   */
  function expandRuleChildren(ruleId, rule, rules, visitedRules) {
    if (visitedRules.has(ruleId)) {
      return null;
    }

    visitedRules.add(ruleId);

    return {
      ...rule,
      children: rule.children.map(childRuleId =>
          expandRuleChildren(childRuleId, rules[childRuleId], rules, visitedRules)),
    };
  }

  return Object.entries(state).reduce((sourcesObj, [sourceId, source]) => {
    const { rules } = source;
    // Log of visited rules in this source. Helps avoid duplication when traversing the
    // descendant rule tree. This Set is unique per source. It will be passed down to
    // be populated with ids of rules once visited. This ensures that only visited rules
    // unique to this source will be skipped and prevents skipping identical rules from
    // other sources (ex: rules with the same selector and the same index).
    const visitedRules = new Set();

    // Build a new collection of sources keyed by source id.
    sourcesObj[sourceId] = {
      ...source,
      // Build a new collection of rules keyed by rule id.
      rules: Object.entries(rules).reduce((rulesObj, [ruleId, rule]) => {
        // Expand the rule's array of child rule ids with the referenced child rules.
        // Skip exposing null values which mean the rule was previously visited as part
        // of an ancestor descendant tree.
        const expandedRule = expandRuleChildren(ruleId, rule, rules, visitedRules);
        if (expandedRule !== null) {
          rulesObj[ruleId] = expandedRule;
        }

        return rulesObj;
      }, {}),
    };

    return sourcesObj;
  }, {});
}

module.exports = {
  getChangesTree,
};
