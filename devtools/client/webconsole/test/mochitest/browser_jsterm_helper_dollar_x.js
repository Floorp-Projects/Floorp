/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
    "$x('.//li')",
    "Array [ li, li ]"
  );
  ok(message, "`$x` worked");

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body)[0]",
    "<li>"
  );
  ok(message, "`$x()` result can be used right away");

  message = await executeAndWaitForMessage(
    hud,
    "$x('count(.//li)', document.body, XPathResult.NUMBER_TYPE)",
    "2"
  );
  ok(message, "$x works as expected with XPathResult.NUMBER_TYPE");

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.STRING_TYPE)",
    "First"
  );
  ok(message, "$x works as expected with XPathResult.STRING_TYPE");

  message = await executeAndWaitForMessage(
    hud,
    "$x('//li[not(@foo)]', document.body, XPathResult.BOOLEAN_TYPE)",
    "true"
  );
  ok(message, "$x works as expected with XPathResult.BOOLEAN_TYPE");

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.UNORDERED_NODE_ITERATOR_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.UNORDERED_NODE_ITERATOR_TYPE"
  );

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ORDERED_NODE_ITERATOR_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.ORDERED_NODE_ITERATOR_TYPE"
  );

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ANY_UNORDERED_NODE_TYPE)",
    "<li>"
  );
  ok(message, "$x works as expected with XPathResult.ANY_UNORDERED_NODE_TYPE");

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.FIRST_ORDERED_NODE_TYPE)",
    "<li>"
  );
  ok(message, "$x works as expected with XPathResult.FIRST_ORDERED_NODE_TYPE");

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE"
  );

  message = await executeAndWaitForMessage(
    hud,
    "$x('.//li', document.body, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE)",
    "Array [ li, li ]"
  );
  ok(
    message,
    "$x works as expected with XPathResult.ORDERED_NODE_SNAPSHOT_TYPE"
  );
});
