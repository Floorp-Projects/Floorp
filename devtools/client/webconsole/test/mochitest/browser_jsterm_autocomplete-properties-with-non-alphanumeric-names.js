/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that properties starting with underscores or dollars can be
// autocompleted (bug 967468).
const TEST_URI = `data:text/html;charset=utf8,test autocompletion with $ or _`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  execute(hud, "var testObject = {$$aaab: '', $$aaac: ''}");

  // Should work with bug 967468.
  await testAutocomplete(hud, "Object.__d");
  await testAutocomplete(hud, "testObject.$$a");

  // Here's when things go wrong in bug 967468.
  await testAutocomplete(hud, "Object.__de");
  await testAutocomplete(hud, "testObject.$$aa");

  // Should work with bug 1207868.
  execute(hud, "let foobar = {a: ''}; const blargh = {a: 1};");
  await testAutocomplete(hud, "foobar");
  await testAutocomplete(hud, "blargh");
  await testAutocomplete(hud, "foobar.a");
  await testAutocomplete(hud, "blargh.a");
});

async function testAutocomplete(hud, inputString) {
  await setInputValueForAutocompletion(hud, inputString);
  const popup = hud.jsterm.autocompletePopup;
  ok(
    popup.itemCount > 0,
    `There's ${popup.itemCount} suggestions for '${inputString}'`
  );
}
