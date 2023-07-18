/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html>
<main>
  <ul>
    <li>First</li>
    <li>Second</li>
  </ul>
  <ul id="myList">
    <li id="myListItem1" class="inMyList">First</li>
    <li id="myListItem2" class="inMyList">Second</li>
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
    "Array(4) [ li, li, li#myListItem1.inMyList, li#myListItem2.inMyList ]"
  );
  ok(message, "`$$('main > ul > li')` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$('main > ul > li').map(el => el.tagName).join(' - ')",
    "LI - LI - LI - LI"
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

  message = await executeAndWaitForResultMessage(
    hud,
    "$$('li', document.querySelector('ul#myList'))",
    "Array [ li#myListItem1.inMyList, li#myListItem2.inMyList ]"
  );
  ok(message, "`$$('li', document.querySelector('ul#myList'))` worked");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$$('li', $(':foo'))",
    "':foo' is not a valid selector"
  );
  ok(message, "`$$('li', $(':foo'))` returns an error message");

  message = await executeAndWaitForResultMessage(
    hud,
    "$$('li', $('div'))",
    "Array(4) [ li, li, li#myListItem1.inMyList, li#myListItem2.inMyList ]"
  );
  ok(message, "`$$('li', $('div'))` worked");
});
