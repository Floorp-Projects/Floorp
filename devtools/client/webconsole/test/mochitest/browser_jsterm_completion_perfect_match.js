/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly in regards to case sensitivity.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test completion perfect match.
  <script>
    x = Object.create(null, Object.getOwnPropertyDescriptors({
      foo: 1,
      foO: 2,
      fOo: 3,
      fOO: 4,
    }));
  </script>`;

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
  const {jsterm} = hud;
  const {autocompletePopup} = jsterm;

  info("Check that filtering the cache works like on the server");
  const onPopUpOpen = autocompletePopup.once("popup-opened");
  EventUtils.sendString("x.");
  await onPopUpOpen;
  is(getAutocompletePopupLabels(autocompletePopup).join(" - "), "foo - foO - fOo - fOO",
    "popup has expected item, in expected order");

  const onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("foO");
  await onAutoCompleteUpdated;
  is(getAutocompletePopupLabels(autocompletePopup).join(" - "), "foO - foo - fOo - fOO",
    "popup has expected item, in expected order");
}

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
