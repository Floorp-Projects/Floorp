/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/states.js */
/* import-globals-from ../../mochitest/role.js */
loadScripts(
  { name: "states.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR }
);

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarProvider: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
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
}

// Check that the URL bar manages accessibility focus appropriately.
async function runTests() {
  registerCleanupFunction(async function() {
    await UrlbarTestUtils.promisePopupClose(window);
    await PlacesUtils.history.clear();
  });

  await PlacesTestUtils.addVisits([
    "http://example1.com/blah",
    "http://example2.com/blah",
    "http://example1.com/",
    "http://example2.com/",
  ]);

  // Ensure initial state.
  await UrlbarTestUtils.promisePopupClose(window);

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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "example",
    fireInputEvent: true,
  });
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring no focus change on backspace");
  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);
  // Wait a tick for a11y events to fire.
  await TestUtils.waitForTick();
  testStates(textBox, STATE_FOCUSED);

  info("Ensuring no focus change on text selection and delete");
  EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
  EventUtils.synthesizeKey("KEY_Delete");
  await UrlbarTestUtils.promiseSearchComplete(window);
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

  info("Ensuring previous arrow selection state doesn't get stale on input");
  focused = waitForEvent(EVENT_FOCUS, textBox);
  EventUtils.sendString("z");
  await focused;
  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);
  testStates(textBox, STATE_FOCUSED);

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
  await UrlbarTestUtils.promiseSearchComplete(window);

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
  await UrlbarTestUtils.promiseSearchComplete(window);

  info("Ensuring autocomplete focus on arrow down (4)");
  focused = waitForEvent(EVENT_FOCUS, isEventForAutocompleteItem);
  EventUtils.synthesizeKey("KEY_ArrowDown");
  event = await focused;
  testStates(event.accessible, STATE_FOCUSED);

  // Arrow down to the last result.
  const resultCount = UrlbarTestUtils.getResultCount(window);
  while (UrlbarTestUtils.getSelectedRowIndex(window) != resultCount - 1) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

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

  if (
    AppConstants.platform == "macosx" &&
    Services.prefs.getBoolPref("widget.macos.native-context-menus", false)
  ) {
    // With native context menus, we do not observe accessibility events and we
    // cannot send synthetic key events to the menu.
    info("Opening and closing context native context menu");
    let contextMenu = gURLBar.querySelector("menupopup");
    let popupshown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gURLBar.querySelector("moz-input-box"), {
      type: "contextmenu",
    });
    await popupshown;
    let popuphidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
    contextMenu.hidePopup();
    await popuphidden;
  } else {
    info(
      "Ensuring context menu gets menu event on launch, and item focus on down"
    );
    let menuEvent = waitForEvent(
      nsIAccessibleEvent.EVENT_MENUPOPUP_START,
      isEventForMenuPopup
    );
    EventUtils.synthesizeMouseAtCenter(gURLBar.querySelector("moz-input-box"), {
      type: "contextmenu",
    });
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
  }
  info("Ensuring address bar is focused after context menu is dismissed.");
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
        type: "test",
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

  // Ensure the tip appears in the expected position.
  matches[1].suggestedIndex = 2;

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
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "example",
    fireInputEvent: true,
  });
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
