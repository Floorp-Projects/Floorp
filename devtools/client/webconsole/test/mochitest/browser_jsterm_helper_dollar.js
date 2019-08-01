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
}
