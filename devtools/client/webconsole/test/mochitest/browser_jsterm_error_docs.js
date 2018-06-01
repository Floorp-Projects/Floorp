/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test error documentation";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  // Check that errors with entries in errordocs.js display links next to their messages.
  const ErrorDocs = require("devtools/server/actors/errordocs");

  const ErrorDocStatements = {
    "JSMSG_BAD_RADIX": "(42).toString(0);",
    "JSMSG_BAD_ARRAY_LENGTH": "([]).length = -1",
    "JSMSG_NEGATIVE_REPETITION_COUNT": "'abc'.repeat(-1);",
    "JSMSG_PRECISION_RANGE": "77.1234.toExponential(-1);",
  };

  for (const [errorMessageName, expression] of Object.entries(ErrorDocStatements)) {
    const title = ErrorDocs.GetURL({ errorMessageName }).split("?")[0];

    jsterm.clearOutput();
    const onMessage = waitForMessage(hud, "RangeError:");
    jsterm.execute(expression);
    const {node} = await onMessage;
    const learnMoreLink = node.querySelector(".learn-more-link");
    ok(learnMoreLink, `There is a [Learn More] link for "${errorMessageName}" error`);
    is(learnMoreLink.title, title, `The link has the expected "${title}" title`);
  }
});
