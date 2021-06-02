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

  // Place the mouse on the top left corner to avoid triggering an highlighter request
  // to the server. See Bug 1535082.
  EventUtils.synthesizeMouse(
    hud.ui.outputNode,
    0,
    0,
    { type: "mousemove" },
    hud.iframeWindow
  );

  let message = await executeAndWaitForMessage(
    hud,
    "$$('main')",
    "Array [ main ]",
    ".result"
  );
  ok(message, "`$$('main')` worked");

  message = await executeAndWaitForMessage(
    hud,
    "$$('main > ul > li')",
    "Array [ li, li ]",
    ".result"
  );
  ok(message, "`$$('main > ul > li')` worked");

  message = await executeAndWaitForMessage(
    hud,
    "$$('main > ul > li').map(el => el.tagName).join(' - ')",
    "LI - LI",
    ".result"
  );
  ok(message, "`$$` result can be used right away");

  message = await executeAndWaitForMessage(
    hud,
    "$$('div')",
    "Array []",
    ".result"
  );
  ok(message, "`$$('div')` returns an empty array");

  message = await executeAndWaitForMessage(
    hud,
    "$$(':foo')",
    "':foo' is not a valid selector",
    ".error"
  );
  ok(message, "`$$(':foo')` returns an error message");
});
