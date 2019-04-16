/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_ADD_RULE_ENABLED,
  UPDATE_HIGHLIGHTED_SELECTOR,
  UPDATE_PRINT_SIMULATION_HIDDEN,
  UPDATE_RULES,
  UPDATE_SOURCE_LINK_ENABLED,
  UPDATE_SOURCE_LINK,
} = require("./index");

module.exports = {

  /**
   * Updates whether or not the add new rule button should be enabled.
   *
   * @param  {Boolean} enabled
   *         Whether or not the add new rule button is enabled.
   */
  updateAddRuleEnabled(enabled) {
    return {
      type: UPDATE_ADD_RULE_ENABLED,
      enabled,
    };
  },

  /**
   * Updates the highlighted selector.
   *
   * @param  {String} highlightedSelector
   *         The selector of the element to be highlighted by the selector highlighter.
   */
  updateHighlightedSelector(highlightedSelector) {
    return {
      type: UPDATE_HIGHLIGHTED_SELECTOR,
      highlightedSelector,
    };
  },

  /**
   * Updates whether or not the print simulation button is hidden.
   *
   * @param  {Boolean} hidden
   *         Whether or not the print simulation button is hidden.
   */
  updatePrintSimulationHidden(hidden) {
    return {
      type: UPDATE_PRINT_SIMULATION_HIDDEN,
      hidden,
    };
  },

  /**
   * Updates the rules state with the new list of CSS rules for the selected element.
   *
   * @param  {Array} rules
   *         Array of Rule objects containing the selected element's CSS rules.
   */
  updateRules(rules) {
    return {
      type: UPDATE_RULES,
      rules,
    };
  },

  /**
   * Updates whether or not the source links are enabled.
   *
   * @param  {Boolean} enabled
   *         Whether or not the source links are enabled.
   */
  updateSourceLinkEnabled(enabled) {
    return {
      type: UPDATE_SOURCE_LINK_ENABLED,
      enabled,
    };
  },

  /**
   * Updates the source link information for a given rule.
   *
   * @param  {String} ruleId
   *         The Rule id of the target rule.
   * @param  {Object} sourceLink
   *         New source link data.
   */
  updateSourceLink(ruleId, sourceLink) {
    return {
      type: UPDATE_SOURCE_LINK,
      ruleId,
      sourceLink,
    };
  },

};
