/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

function isEventForAutocompleteItem(event) {
  return event.accessible.role == ROLE_COMBOBOX_OPTION;
}

function isEventForButton(event) {
  return event.accessible.role == ROLE_PUSHBUTTON;
}

function isEventForOneOffEngine(event) {
  let parent = event.accessible.parent;
  return (
    event.accessible.role == ROLE_PUSHBUTTON &&
    parent &&
    parent.role == ROLE_GROUPING &&
    parent.name
  );
}

function isEventForMenuPopup(event) {
  return event.accessible.role == ROLE_MENUPOPUP;
}

function isEventForMenuItem(event) {
  return event.accessible.role == ROLE_MENUITEM;
}

function isEventForTipButton(event) {
  let parent = event.accessible.parent;
  return (
    event.accessible.role == ROLE_PUSHBUTTON &&
    parent &&
    parent.role == ROLE_GROUPING &&
    parent.name
  );
}

/**
 * Wait for an autocomplete search to finish.
 * This is necessary to ensure predictable results, as these searches are
 * async. Pressing down arrow will use results from the previous input if the
 * search isn't finished yet.
 */
function waitForSearchFinish() {
  return Promise.all([
    gURLBar.lastQueryContextPromise,
    BrowserTestUtils.waitForCondition(() => gURLBar.view.isOpen),
  ]);
}

/**
 * A test provider.
 */
class TipTestProvider extends UrlbarProvider {
  constructor(matches) {
    super();
    this._matches = matches;
  }
  get name() {
    return "TipTestProvider";
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  isActive(context) {
    return true;
  }
  isRestricting(context) {
    return true;
  }
  async startQuery(context, addCallback) {
    this._context = context;
    for (const match of this._matches) {
      addCallback(this, match);
    }
  }
  cancelQuery(context) {}
  pickResult(result, details) {}
}

// Check that the URL bar manages accessibility focus appropriately.
async function runTests() {
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  await PlacesTestUtils.addVisits([
    { uri: makeURI("http://example1.com/blah") },
    { uri: makeURI("http://example2.com/blah") },
    { uri: makeURI("http://example1.com/") },
    { uri: makeURI("http://example2.com/") },
  ]);

  let focused = waitForEvent(
    EVENT_FOCUS,
    event => event.accessible.role == ROLE_ENTRY
  );
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
  EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
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
    EventUtils.synthesizeKey("n", { ctrlKey: true });
    event = await focused;
    testStates(event.accessible, STATE_FOCUSED);

    info("Ensuring focus of another autocomplete item on ctrl-p");
    focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
    EventUtils.synthesizeKey("p", { ctrlKey: true });
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
  gURLBar.view.close();
  // On Mac, down arrow when not at the end of the field moves to the end.
  // Move back to the end so the next press of down arrow opens the popup.
  EventUtils.synthesizeKey("KEY_ArrowRight");

  info("Ensuring autocomplete focus on down arrow (2)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring autocomplete focus on arrow up for search settings button");
  focused = waitForEvent(EVENT_FOCUS, isEventForButton);
  EventUtils.synthesizeKey("KEY_ArrowUp");
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

  info("Ensuring autocomplete focus on arrow down (4)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring one-off search button focus on arrow down");
  focused = waitForEvent(EVENT_FOCUS, isEventForOneOffEngine);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring autocomplete focus on arrow up");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowUp");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus on text selection");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
  await focused;
  testStates(textBox, STATE_FOCUSED);

  if (AppConstants.platform == "macosx") {
    // On Mac, ctrl-n after arrow left/right does not re-open the popup.
    // Type some text so the next press of ctrl-n opens the popup.
    EventUtils.sendString("ple");

    info("Ensuring autocomplete focus on ctrl-n");
    focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
    EventUtils.synthesizeKey("n", { ctrlKey: true });
    event = await focused;
    testStates(event.accessible, STATE_FOCUSED);
  }

  info(
    "Ensuring context menu gets menu event on launch, item focus on down, and address bar focus on escape."
  );
  let menuEvent = waitForEvent(
    nsIAccessibleEvent.EVENT_MENUPOPUP_START,
    isEventForMenuPopup
  );
  EventUtils.sendMouseEvent(
    { type: "contextmenu" },
    gURLBar.querySelector("moz-input-box")
  );
  await menuEvent;

  focused = waitForEvent(EVENT_FOCUS, isEventForMenuItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  focused = waitForEvent(EVENT_FOCUS, textBox);
  let closed = waitForEvent(
    nsIAccessibleEvent.EVENT_MENUPOPUP_END,
    isEventForMenuPopup
  );
  EventUtils.synthesizeKey("KEY_Escape");
  await closed;
  await focused;
  testStates(textBox, STATE_FOCUSED);
}

// We test TIP results in their own test so the spoofed results don't interfere
// with the main test.
async function runTipTests() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        icon: "",
        text: "This is a test intervention.",
        buttonText: "Done",
        data: "test",
        helpUrl: "about:blank",
        buttonUrl: "about:mozilla",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/b" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];

  let provider = new TipTestProvider(matches);
  UrlbarProvidersManager.registerProvider(provider);

  registerCleanupFunction(async function() {
    UrlbarProvidersManager.unregisterProvider(provider);
  });

  let focused = waitForEvent(
    EVENT_FOCUS,
    event => event.accessible.role == ROLE_ENTRY
  );
  gURLBar.focus();
  let event = await focused;
  let textBox = event.accessible;

  EventUtils.synthesizeKey("KEY_Escape");
  EventUtils.synthesizeKey("KEY_Escape");

  info("Ensuring no focus change when first text is typed");
  EventUtils.sendString("example");
  await waitForSearchFinish();
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring autocomplete focus on down arrow (1)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring the tip button is focused on down arrow");
  info("Also ensuring that the tip button is a part of a labelled group");
  focused = waitForEvent(EVENT_FOCUS, isEventForTipButton);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring the help button is focused on down arrow");
  info("Also ensuring that the help button is a part of a labelled group");
  focused = waitForEvent(EVENT_FOCUS, isEventForTipButton);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring autocomplete focus on down arrow (2)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring the help button is focused on up arrow");
  focused = waitForEvent(EVENT_FOCUS, isEventForTipButton);
  EventUtils.synthesizeKey("KEY_ArrowUp");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  info("Ensuring text box focus on left arrow, and not back to the tip button");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await focused;
  testStates(textBox, STATE_FOCUSED);
}

addAccessibleTask(``, runTests);
addAccessibleTask(``, runTipTests);
