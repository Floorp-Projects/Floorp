/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,<!DOCTYPE html>Test error documentation";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Check that errors with entries in errordocs.js display links next to their messages.
  const ErrorDocs = require("resource://devtools/server/actors/errordocs.js");

  const ErrorDocStatements = {
    JSMSG_BAD_RADIX: "(42).toString(0);",
    JSMSG_BAD_ARRAY_LENGTH: "([]).length = -1",
    JSMSG_NEGATIVE_REPETITION_COUNT: "'abc'.repeat(-1);",
    JSMSG_PRECISION_RANGE: "77.1234.toExponential(-1);",
  };

  for (const [errorMessageName, expression] of Object.entries(
    ErrorDocStatements
  )) {
    const errorUrl = ErrorDocs.GetURL({ errorMessageName });
    const title = errorUrl.split("?")[0];

    await clearOutput(hud);

    const { node } = await executeAndWaitForErrorMessage(
      hud,
      expression,
      "RangeError:"
    );
    const learnMoreLink = node.querySelector(".learn-more-link");
    ok(
      learnMoreLink,
      `There is a [Learn More] link for "${errorMessageName}" error`
    );
    is(
      learnMoreLink.title,
      title,
      `The link has the expected "${title}" title`
    );
    is(
      learnMoreLink.href,
      errorUrl,
      `The link has the expected "${errorUrl}" href value`
    );
  }
});
