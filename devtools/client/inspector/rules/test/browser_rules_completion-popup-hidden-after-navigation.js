/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the ruleview autocomplete popup is hidden after page navigation.

const TEST_URI = "<h1 style='font: 24px serif'></h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view, testActor} = await openRuleView();

  info("Test autocompletion popup is hidden after page navigation");

  info("Selecting the test node");
  await selectNode("h1", inspector);

  info("Focusing the css property editable field");
  const propertyName = view.styleDocument
    .querySelectorAll(".ruleview-propertyname")[0];
  const editor = await focusEditableField(view, propertyName);

  info("Pressing key VK_DOWN");
  const onSuggest = once(editor.input, "keypress");
  const onPopupOpened = once(editor.popup, "popup-opened");

  EventUtils.synthesizeKey("VK_DOWN", {}, view.styleWindow);

  info("Waiting for autocomplete popup to be displayed");
  await onSuggest;
  await onPopupOpened;

  ok(view.popup && view.popup.isOpen, "Popup should be opened");

  info("Reloading the page");
  await reloadPage(inspector, testActor);

  ok(!(view.popup && view.popup.isOpen), "Popup should be closed");
});
