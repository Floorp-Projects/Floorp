/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(
  this,
  "getTabPrefs",
  "resource://devtools/shared/indentation.js",
  true
);

const {
  getSourceForDisplay,
} = require("resource://devtools/client/inspector/changes/utils/changes-utils.js");

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
  const { sourceIds: sourceIdsFilter = [], ruleIds: rulesIdsFilter = [] } =
    filter;
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
        expandRuleChildren(childRuleId, rules[childRuleId], rules, visitedRules)
      ),
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
            const expandedRule = expandRuleChildren(
              ruleId,
              rule,
              rules,
              visitedRules
            );
            if (expandedRule !== null) {
              rulesObj[ruleId] = expandedRule;
            }

            return rulesObj;
          }, {}),
      };

      return sourcesObj;
    }, {});
}

/**
 * Build the CSS text of a stylesheet with the changes aggregated in the Redux state.
 * If filters for rule id or source id are provided, restrict the changes to the matching
 * sources and rules.
 *
 * Code comments with the source origin are put above of the CSS rule (or group of
 * rules). Removed CSS declarations are written commented out. Added CSS declarations are
 * written as-is.
 *
 * @param  {Object} state
 *         Redux slice for tracked changes.
 * @param  {Object} filter
 *         Object with optional source and rule filters. See getChangesTree()
 * @return {String}
 *         CSS stylesheet text.
 */

// For stylesheet sources, the stylesheet filename and full path are used:
//
// /* styles.css | https://example.com/styles.css */
//
// .selector {
//  /* property: oldvalue; */
//  property: value;
// }

// For inline stylesheet sources, the stylesheet index and host document URL are used:
//
// /* Inline #1 | https://example.com */
//
// .selector {
//  /* property: oldvalue; */
//  property: value;
// }

// For element style attribute sources, the unique selector generated for the element
// and the host document URL are used:
//
// /* Element (div) | https://example.com */
//
// div:nth-child(1) {
//  /* property: oldvalue; */
//  property: value;
// }
function getChangesStylesheet(state, filter) {
  const changeTree = getChangesTree(state, filter);
  // Get user prefs about indentation style.
  const { indentUnit, indentWithTabs } = getTabPrefs();
  const indentChar = indentWithTabs
    ? "\t".repeat(indentUnit)
    : " ".repeat(indentUnit);

  /**
   * If the rule has just one item in its array of selector versions, return it as-is.
   * If it has more than one, build a string using the first selector commented-out
   * and the last selector as-is. This indicates that a rule's selector has changed.
   *
   * @param  {Array} selectors
   *         History of selector versions if changed over time.
   *         Array with a single item (the original selector) if never changed.
   * @param  {Number} level
   *         Level of nesting within a CSS rule tree.
   * @return {String}
   */
  function writeSelector(selectors = [], level) {
    const indent = indentChar.repeat(level);
    let selectorText;
    switch (selectors.length) {
      case 0:
        selectorText = "";
        break;
      case 1:
        selectorText = `${indent}${selectors[0]}`;
        break;
      default:
        selectorText =
          `${indent}/* ${selectors[0]} { */\n` +
          `${indent}${selectors[selectors.length - 1]}`;
    }

    return selectorText;
  }

  function writeRule(ruleId, rule, level) {
    // Write nested rules, if any.
    let ruleBody = rule.children.reduce((str, childRule) => {
      str += writeRule(childRule.ruleId, childRule, level + 1);
      return str;
    }, "");

    // Write changed CSS declarations.
    ruleBody += writeDeclarations(rule.remove, rule.add, level + 1);

    const indent = indentChar.repeat(level);
    const selectorText = writeSelector(rule.selectors, level);
    return `\n${selectorText} {${ruleBody}\n${indent}}`;
  }

  function writeDeclarations(remove = [], add = [], level) {
    const indent = indentChar.repeat(level);
    const removals = remove
      // Sort declarations in the order in which they exist in the original CSS rule.
      .sort((a, b) => a.index > b.index)
      .reduce((str, { property, value }) => {
        str += `\n${indent}/* ${property}: ${value}; */`;
        return str;
      }, "");

    const additions = add
      // Sort declarations in the order in which they exist in the original CSS rule.
      .sort((a, b) => a.index > b.index)
      .reduce((str, { property, value }) => {
        str += `\n${indent}${property}: ${value};`;
        return str;
      }, "");

    return removals + additions;
  }

  // Iterate through all sources in the change tree and build a CSS stylesheet string.
  return Object.entries(changeTree).reduce(
    (stylesheetText, [sourceId, source]) => {
      const { href, rules } = source;
      // Write code comment with source origin
      stylesheetText += `\n/* ${getSourceForDisplay(source)} | ${href} */\n`;
      // Write CSS rules
      stylesheetText += Object.entries(rules).reduce((str, [ruleId, rule]) => {
        // Add a new like only after top-level rules (level == 0)
        str += writeRule(ruleId, rule, 0) + "\n";
        return str;
      }, "");

      return stylesheetText;
    },
    ""
  );
}

module.exports = {
  getChangesTree,
  getChangesStylesheet,
};
