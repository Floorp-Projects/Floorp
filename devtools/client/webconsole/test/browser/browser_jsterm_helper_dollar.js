/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html>
<main>
  <ul>
    <li>First</li>
    <li>Second</li>
  </ul>
  <aside>Sidebar</aside>
</main>
`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForResultMessage(
    hud,
    "$('main')",
    "<main>"
  );
  ok(message, "`$('main')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$('main > ul > li')",
    "<li>"
  );
  ok(message, "`$('main > ul > li')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$('main > ul > li').tagName",
    "LI"
  );
  ok(message, "`$` result can be used right away");

  message = await executeAndWaitForResultMessage(hud, "$('div')", "null");
  ok(message, "`$('div')` does return null");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$(':foo')",
    "':foo' is not a valid selector"
  );
  ok(message, "`$(':foo')` returns an error message");
});
