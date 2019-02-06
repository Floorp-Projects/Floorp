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
 * @param {Object} filter
 *        Object with optional filters to use. Has the following properties:
 *        - sourceIds: {Array}
 *          Use only subtrees of sources matching source ids from this array.
 *        - ruleIds: {Array}
 *          Use only rules matching rule ids from this array. If the array includes ids
 *          of ancestor rules (@media, @supports), their nested rules will be included.
 * @return {Object}
 */
function getChangesTree(state, filter = {}) {
  // Use or assign defaults of sourceId and ruleId arrays by which to filter the tree.
  const { sourceIds: sourceIdsFilter = [], ruleIds: rulesIdsFilter = [] } = filter;
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

  return Object.entries(state)
    .filter(([sourceId, source]) => {
      // Use only matching sources if an array to filter by was provided.
      if (sourceIdsFilter.length) {
        return sourceIdsFilter.includes(sourceId);
      }

      return true;
    })
    .reduce((sourcesObj, [sourceId, source]) => {
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
        rules: Object.entries(rules)
          .filter(([ruleId, rule]) => {
            // Use only matching rules if an array to filter by was provided.
            if (rulesIdsFilter.length) {
              return rulesIdsFilter.includes(ruleId);
            }

            return true;
          })
          .reduce((rulesObj, [ruleId, rule]) => {
            // Expand the rule's array of child rule ids with the referenced child rules.
            // Skip exposing null values which mean the rule was previously visited
            // as part of an ancestor descendant tree.
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
