/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that getChangesStylesheet() serializes tracked changes from nested CSS rules
// into the expected stylesheet format.

const {
  getChangesStylesheet,
} = require("devtools/client/inspector/changes/selectors/changes");

const { CHANGES_STATE } = require("resource://test/mocks");

// Wrap multi-line string in backticks to ensure exact check in test, including new lines.
const STYLESHEET_FOR_ANCESTOR = `
/* Inline #0 | http://localhost:5000/at-rules-nested.html */

@media (min-width: 50em) {
  @supports (display: grid) {
    body {
      /* background-color: royalblue; */
      background-color: red;
    }
  }
}
`;

// Wrap multi-line string in backticks to ensure exact check in test, including new lines.
const STYLESHEET_FOR_DESCENDANT = `
/* Inline #0 | http://localhost:5000/at-rules-nested.html */

body {
  /* background-color: royalblue; */
  background-color: red;
}
`;

add_test(() => {
  info(
    "Check stylesheet generated for the first ancestor in the CSS rule tree."
  );
  equal(
    getChangesStylesheet(CHANGES_STATE),
    STYLESHEET_FOR_ANCESTOR,
    "Stylesheet includes all ancestors."
  );

  info(
    "Check stylesheet generated for the last descendant in the CSS rule tree."
  );
  const filter = { sourceIds: ["source1"], ruleIds: ["rule3"] };
  equal(
    getChangesStylesheet(CHANGES_STATE, filter),
    STYLESHEET_FOR_DESCENDANT,
    "Stylesheet includes just descendant."
  );

  run_next_test();
});
