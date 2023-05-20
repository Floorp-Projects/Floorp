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
  // to the server. See Bug 1531572.
  EventUtils.synthesizeMouse(
    hud.ui.outputNode,
    0,
    0,
    { type: "mousemove" },
    hud.iframeWindow
  );

  let message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li')",
    "Array [ li, li ]"
  );
  ok(message, "`$x` worked");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body)[0]",
    "<li>"
  );
  ok(message, "`$x()` result can be used right away");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('count(.//li)', document.body, XPathResult.NUMBER_TYPE)",
    "2"
  );
  ok(message, "$x works as expected with XPathResult.NUMBER_TYPE");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('count(.//li)', document.body, 'number')",
    "2"
  );
  ok(message, "$x works as expected number type");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.STRING_TYPE)",
    "First"
  );
  ok(message, "$x works as expected with XPathResult.STRING_TYPE");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, 'string')",
    "First"
  );
  ok(message, "$x works as expected with string type");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('//li[not(@foo)]', document.body, XPathResult.BOOLEAN_TYPE)",
    "true"
  );
  ok(message, "$x works as expected with XPathResult.BOOLEAN_TYPE");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('//li[not(@foo)]', document.body, 'bool')",
    "true"
  );
  ok(message, "$x works as expected with bool type");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.UNORDERED_NODE_ITERATOR_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.UNORDERED_NODE_ITERATOR_TYPE"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, 'nodes')",
    "Array [ li, li ]"
  );
  ok(message, "$x works as expected with nodes type");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ORDERED_NODE_ITERATOR_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.ORDERED_NODE_ITERATOR_TYPE"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ANY_UNORDERED_NODE_TYPE)",
    "<li>"
  );
  ok(message, "$x works as expected with XPathResult.ANY_UNORDERED_NODE_TYPE");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.FIRST_ORDERED_NODE_TYPE)",
    "<li>"
  );
  ok(message, "$x works as expected with XPathResult.FIRST_ORDERED_NODE_TYPE");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, 'node')",
    "<li>"
  );
  ok(message, "$x works as expected with node type");

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.ORDERED_NODE_SNAPSHOT_TYPE"
  );
});
