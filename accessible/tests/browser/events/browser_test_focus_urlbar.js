/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "states.js", dir: MOCHITESTS_DIR },
            { name: "role.js", dir: MOCHITESTS_DIR });
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

function isEventForAutocompleteItem(event) {
  return event.accessible.role == ROLE_COMBOBOX_OPTION;
}

/**
 * Wait for an autocomplete search to finish.
 * This is necessary to ensure predictable results, as these searches are
 * async. Pressing down arrow will use results from the previous input if the
 * search isn't finished yet.
 */
function waitForSearchFinish() {
  if (UrlbarPrefs.get("quantumbar")) {
    return Promise.all([
      gURLBar.lastQueryContextPromise,
      BrowserTestUtils.waitForCondition(() => gURLBar.view.isOpen)
    ]);
  }
  return BrowserTestUtils.waitForCondition(() =>
    (gURLBar.popupOpen && gURLBar.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH),
    "Waiting for search to complete");
}

// Check that the URL bar manages accessibility focus appropriately.
async function runTests() {
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await PlacesTestUtils.addVisits([
    {uri: makeURI("http://example1.com/blah")},
    {uri: makeURI("http://example2.com/blah")},
    {uri: makeURI("http://example1.com/")},
    {uri: makeURI("http://example2.com/")}
  ]);

  let focused = waitForEvent(EVENT_FOCUS,
                             event => event.accessible.role == ROLE_ENTRY);
  gURLBar.focus();
  let event = await focused;
  let textBox = event.accessible;
  // Ensure the URL bar is ready for a new URL to be typed.
  // Sometimes, when this test runs, the existing text isn't selected when the
  // URL bar is focused. Pressing escape twice ensures that the popup is
  // closed and that the existing text is selected.
  EventUtils.synthesizeKey("KEY_Escape");
  EventUtils.synthesizeKey("KEY_Escape");

  info("Ensuring no focus change when first text is typed");
  EventUtils.sendString("example");
  await waitForSearchFinish();
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring no focus change on backspace");
  EventUtils.synthesizeKey("KEY_Backspace");
  await waitForSearchFinish();
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring no focus change on text selection and delete");
  EventUtils.synthesizeKey("KEY_ArrowLeft", {shiftKey: true});
  EventUtils.synthesizeKey("KEY_Delete");
  await waitForSearchFinish();
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring autocomplete focus on down arrow (1)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring focus of another autocomplete item on down arrow");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  if (AppConstants.platform == "macosx") {
    info("Ensuring focus of another autocomplete item on ctrl-n");
    focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
    EventUtils.synthesizeKey("n", {ctrlKey: true});
    event = await focused;
    testStates(event.accessible, STATE_FOCUSED);

    info("Ensuring focus of another autocomplete item on ctrl-p");
    focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
    EventUtils.synthesizeKey("p", {ctrlKey: true});
    event = await focused;
    testStates(event.accessible, STATE_FOCUSED);
  }

  info("Ensuring focus of another autocomplete item on up arrow");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowUp");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus on left arrow");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await focused;
  testStates(textBox, STATE_FOCUSED);
  if (UrlbarPrefs.get("quantumbar")) {
    gURLBar.view.close();
  }
  // On Mac, down arrow when not at the end of the field moves to the end.
  // Move back to the end so the next press of down arrow opens the popup.
  EventUtils.synthesizeKey("KEY_ArrowRight");

  info("Ensuring autocomplete focus on down arrow (2)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus when text is typed");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.sendString("z");
  await focused;
  testStates(textBox, STATE_FOCUSED);
  EventUtils.synthesizeKey("KEY_Backspace");
  await waitForSearchFinish();

  info("Ensuring autocomplete focus on down arrow (3)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus on backspace");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.synthesizeKey("KEY_Backspace");
  await focused;
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring autocomplete focus on arrow down & up");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  // With the quantumbar enabled, we only get one result here, and arrow down
  // selects a one-off search button. We arrow back up to re-select the
  // autocomplete result.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus on text selection");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.synthesizeKey("KEY_ArrowLeft", {shiftKey: true});
  await focused;
  testStates(textBox, STATE_FOCUSED);

  if (AppConstants.platform == "macosx") {
    // On Mac, ctrl-n after arrow left/right does not re-open the popup.
    // Type some text so the next press of ctrl-n opens the popup.
    EventUtils.sendString("ple");

    info("Ensuring autocomplete focus on ctrl-n");
    focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
    EventUtils.synthesizeKey("n", {ctrlKey: true});
    event = await focused;
    testStates(event.accessible, STATE_FOCUSED);
  }
}

addAccessibleTask(``, runTests);
