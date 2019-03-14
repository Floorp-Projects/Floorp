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
  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  let onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm.execute("$x('.//li')");
  let message = await onMessage;
  ok(message, "`$x` worked");

  onMessage = waitForMessage(hud, "<li>");
  jsterm.execute("$x('.//li', document.body)[0]");
  message = await onMessage;
  ok(message, "`$x()` result can be used right away");

  onMessage = waitForMessage(hud, "2");
  jsterm.execute("$x('count(.//li)', document.body, XPathResult.NUMBER_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.NUMBER_TYPE");

  onMessage = waitForMessage(hud, "First");
  jsterm.execute("$x('.//li', document.body, XPathResult.STRING_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.STRING_TYPE");

  onMessage = waitForMessage(hud, "true");
  jsterm.execute("$x('//li[not(@foo)]', document.body, XPathResult.BOOLEAN_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.BOOLEAN_TYPE");

  onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.UNORDERED_NODE_ITERATOR_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.UNORDERED_NODE_ITERATOR_TYPE");

  onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.ORDERED_NODE_ITERATOR_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.ORDERED_NODE_ITERATOR_TYPE");

  onMessage = waitForMessage(hud, "<li>");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.ANY_UNORDERED_NODE_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.ANY_UNORDERED_NODE_TYPE");

  onMessage = waitForMessage(hud, "<li>");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.FIRST_ORDERED_NODE_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.FIRST_ORDERED_NODE_TYPE");

  onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE");

  onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm
  .execute("$x('.//li', document.body, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE)");
  message = await onMessage;
  ok(message, "$x works as expected with XPathResult.ORDERED_NODE_SNAPSHOT_TYPE");
}
