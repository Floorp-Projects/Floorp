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
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  let onMessage = waitForMessage(hud, "Array [ main ]");
  jsterm.execute("$$('main')");
  let message = await onMessage;
  ok(message, "`$$('main')` worked");

  onMessage = waitForMessage(hud, "Array [ li, li ]");
  jsterm.execute("$$('main > ul > li')");
  message = await onMessage;
  ok(message, "`$$('main > ul > li')` worked");

  onMessage = waitForMessage(hud, "LI - LI");
  jsterm.execute("$$('main > ul > li').map(el => el.tagName).join(' - ')");
  message = await onMessage;
  ok(message, "`$$` result can be used right away");

  onMessage = waitForMessage(hud, "Array []");
  jsterm.execute("$$('div')");
  message = await onMessage;
  ok(message, "`$$('div')` returns an empty array");
}
