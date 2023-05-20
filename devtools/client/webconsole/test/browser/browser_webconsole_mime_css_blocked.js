/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that non-CSS parser errors get displayed by default.

"use strict";

const CSS_URI = "data:text/bogus,foo";
const TEST_URI = `data:text/html,<!DOCTYPE html><link rel="stylesheet" href="${CSS_URI}">`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const MSG = `The stylesheet ${CSS_URI} was not loaded because its MIME type, “text/bogus”, is not “text/css”`;
  await waitFor(() => findErrorMessage(hud, MSG), "", 100);
  ok(true, "MIME type error displayed");
});
