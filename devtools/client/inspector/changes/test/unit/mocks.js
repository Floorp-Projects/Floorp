/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable comma-dangle */

"use strict";

/**
 * Snapshot of the Redux state for the Changes panel.
 *
 * It corresponds to the tracking of a single property value change (background-color)
 * within a deeply nested CSS at-rule structure from an inline stylesheet:
 *
 * @media (min-width: 50em) {
 *   @supports (display: grid) {
 *     body {
 *       - background-color: royalblue;
 *       + background-color: red;
 *     }
 *   }
 * }
 */
module.exports.CHANGES_STATE = {
  source1: {
    type: "inline",
    href: "http://localhost:5000/at-rules-nested.html",
    id: "source1",
    index: 0,
    isFramed: false,
    rules: {
      rule1: {
        selectors: ["@media (min-width: 50em)"],
        ruleId: "rule1",
        add: [],
        remove: [],
        children: ["rule2"],
      },
      rule2: {
        selectors: ["@supports (display: grid)"],
        ruleId: "rule2",
        add: [],
        remove: [],
        children: ["rule3"],
        parent: "rule1",
      },
      rule3: {
        selectors: ["body"],
        ruleId: "rule3",
        add: [
          {
            property: "background-color",
            value: "red",
            index: 0,
          },
        ],
        remove: [
          {
            property: "background-color",
            value: "royalblue",
            index: 0,
          },
        ],
        children: [],
        parent: "rule2",
      },
    },
  },
};
