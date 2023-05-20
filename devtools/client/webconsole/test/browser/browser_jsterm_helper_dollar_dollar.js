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

  // Place the mouse on the top left corner to avoid triggering an highlighter request
  // to the server. See Bug 1535082.
  EventUtils.synthesizeMouse(
    hud.ui.outputNode,
    0,
    0,
    { type: "mousemove" },
    hud.iframeWindow
  );

  let message = await executeAndWaitForResultMessage(
    hud,
    "$$('main')",
    "Array [ main ]"
  );
  ok(message, "`$$('main')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$('main > ul > li')",
    "Array [ li, li ]"
  );
  ok(message, "`$$('main > ul > li')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$('main > ul > li').map(el => el.tagName).join(' - ')",
    "LI - LI"
  );
  ok(message, "`$$` result can be used right away");

  message = await executeAndWaitForResultMessage(hud, "$$('div')", "Array []");
  ok(message, "`$$('div')` returns an empty array");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$$(':foo')",
    "':foo' is not a valid selector"
  );
  ok(message, "`$$(':foo')` returns an error message");
});
