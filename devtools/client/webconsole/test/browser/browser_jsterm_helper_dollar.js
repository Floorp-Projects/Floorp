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

  message = await executeAndWaitForResultMessage(
    hud,
    "$('li', document.querySelector('ul#myList'))",
    '<li id="myListItem1" class="inMyList">'
  );
  ok(message, "`$('li', document.querySelector('ul#myList'))` worked");

  message = await executeAndWaitForErrorMessage(
    hud,
    "$('li', $(':foo'))",
    "':foo' is not a valid selector"
  );
  ok(message, "`$('li', $(':foo'))` returns an error message");

  message = await executeAndWaitForResultMessage(
    hud,
    "$('li', $('div'))",
    "<li>"
  );
  ok(message, "`$('li', $('div'))` worked");
});
