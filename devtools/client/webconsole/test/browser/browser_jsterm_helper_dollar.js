/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,
<main>
  <ul>
    <li>First</li>
    <li>Second</li>
  </ul>
  <aside>Sidebar</aside>
</main>
`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForMessage(
    hud,
    "$('main')",
    "<main>",
    ".result"
  );
  ok(message, "`$('main')` worked");

  message = await executeAndWaitForMessage(
    hud,
    "$('main > ul > li')",
    "<li>",
    ".result"
  );
  ok(message, "`$('main > ul > li')` worked");

  message = await executeAndWaitForMessage(
    hud,
    "$('main > ul > li').tagName",
    "LI",
    ".result"
  );
  ok(message, "`$` result can be used right away");

  message = await executeAndWaitForMessage(hud, "$('div')", "null", ".result");
  ok(message, "`$('div')` does return null");

  message = await executeAndWaitForMessage(
    hud,
    "$(':foo')",
    "':foo' is not a valid selector",
    ".error"
  );
  ok(message, "`$(':foo')` returns an error message");
});
