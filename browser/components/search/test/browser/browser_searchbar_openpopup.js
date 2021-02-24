// Tests that the suggestion popup appears at the right times in response to
// focus and user events (mouse, keyboard, drop).

const searchPopup = document.getElementById("PopupSearchAutoComplete");
const kValues = ["long text", "long text 2", "long text 3"];

async function endCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") != "true") {
    return true;
  }
  let eventPromise = BrowserTestUtils.waitForEvent(
    aWindow.gNavToolbox,
    "aftercustomization"
  );
  aWindow.gCustomizeMode.exit();
  return eventPromise;
}

async function startCustomizing(aWindow = window) {
  if (aWindow.document.documentElement.getAttribute("customizing") == "true") {
    return true;
  }
  let eventPromise = BrowserTestUtils.waitForEvent(
    aWindow.gNavToolbox,
    "customizationready"
  );
  aWindow.gCustomizeMode.enter();
  return eventPromise;
}

let searchbar;
let textbox;
let searchIcon;
let goButton;

add_task(async function setup() {
  searchbar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
  textbox = searchbar.textbox;
  searchIcon = searchbar.querySelector(".searchbar-search-button");
  goButton = searchbar.querySelector(".search-go-button");

  let defaultEngine = await Services.search.getDefault();
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine.xml"
  );
  await Services.search.setDefault(engine);

  // First cleanup the form history in case other tests left things there.
  await new Promise((resolve, reject) => {
    info("cleanup the search history");
    searchbar.FormHistory.update(
      { op: "remove", fieldname: "searchbar-history" },
      { handleCompletion: resolve, handleError: reject }
    );
  });

  await new Promise((resolve, reject) => {
    info("adding search history values: " + kValues);
    let addOps = kValues.map(value => {
      return { op: "add", fieldname: "searchbar-history", value };
    });
    searchbar.FormHistory.update(addOps, {
      handleCompletion: resolve,
      handleError: reject,
    });
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
    gCUITestUtils.removeSearchBar();
  });
});

// Adds a task that shouldn't show the search suggestions popup.
function add_no_popup_task(task) {
  add_task(async function() {
    let sawPopup = false;
    function listener() {
      sawPopup = true;
    }

    info("Entering test " + task.name);
    searchPopup.addEventListener("popupshowing", listener);
    await task();
    searchPopup.removeEventListener("popupshowing", listener);
    ok(!sawPopup, "Shouldn't have seen the suggestions popup");
    info("Leaving test " + task.name);
  });
}

// Simulates the full set of events for a context click
function context_click(target) {
  for (let event of ["mousedown", "contextmenu", "mouseup"]) {
    EventUtils.synthesizeMouseAtCenter(target, { type: event, button: 2 });
  }
}

// Right clicking the icon should not open the popup.
add_no_popup_task(async function open_icon_context() {
  gURLBar.focus();
  let toolbarPopup = document.getElementById("toolbar-context-menu");

  let promise = promiseEvent(toolbarPopup, "popupshown");
  context_click(searchIcon);
  await promise;

  promise = promiseEvent(toolbarPopup, "popuphidden");
  toolbarPopup.hidePopup();
  await promise;
});

// With no text in the search box left clicking the icon should open the popup.
// Clicking the icon again should hide the popup and not show it again.
add_task(async function open_empty() {
  gURLBar.focus();

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Clicking icon");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await promise;
  is(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should only show the settings"
  );
  is(textbox.mController.searchString, "", "Should be an empty search string");

  // By giving the textbox some text any next attempt to open the search popup
  // from the click handler will try to search for this text.
  textbox.value = "foo";

  promise = promiseEvent(searchPopup, "popuphidden");

  info("Hiding popup");
  await EventUtils.promiseNativeMouseEventAndWaitForEvent({
    type: "click",
    target: searchIcon,
    atCenter: true,
    eventTypeToWait: "mouseup",
  });
  await promise;

  is(
    textbox.mController.searchString,
    "",
    "Should not have started to search for the new text"
  );

  // Cancel the search if it started.
  if (textbox.mController.searchString != "") {
    textbox.mController.stopSearch();
  }

  textbox.value = "";
});

// With no text in the search box left clicking it should not open the popup.
add_no_popup_task(function click_doesnt_open_popup() {
  gURLBar.focus();

  EventUtils.synthesizeMouseAtCenter(textbox, {});
  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 0, "Should have selected all of the text");
});

// Left clicking in a non-empty search box when unfocused should focus it and open the popup.
add_task(async function click_opens_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;

  textbox.value = "";
});

add_task(async function open_empty_hiddenOneOffs() {
  // Disable all the engines but the current one and check the oneoffs.
  let defaultEngine = await Services.search.getDefault();
  let engines = (await Services.search.getVisibleEngines()).filter(
    e => e.name != defaultEngine.name
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.hiddenOneOffs", engines.map(e => e.name).join(",")]],
  });

  let oneOffButtons = searchPopup.searchOneOffsContainer.querySelector(
    ".search-panel-one-offs"
  );
  textbox.value = "foo";
  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;

  Assert.ok(
    oneOffButtons.hasAttribute("hidden"),
    "The one-offs buttons should have the hidden attribute."
  );
  Assert.ok(
    BrowserTestUtils.is_hidden(oneOffButtons),
    "The one-off buttons should be hidden."
  );

  promise = promiseEvent(searchPopup, "popuphidden");

  info("Hiding popup");
  await EventUtils.promiseNativeMouseEventAndWaitForEvent({
    type: "click",
    target: searchIcon,
    atCenter: true,
    eventTypeToWait: "mouseup",
  });
  await promise;

  await SpecialPowers.popPrefEnv();
  textbox.value = "";
});

// Right clicking in a non-empty search box when unfocused should open the edit context menu.
add_no_popup_task(async function right_click_doesnt_open_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  // Can't wait for an event on the actual menu since it is created
  // lazily the first time it is displayed.
  let promise = new Promise(resolve => {
    let listener = event => {
      if (searchbar._menupopup && event.target == searchbar._menupopup) {
        resolve(searchbar._menupopup);
      }
    };
    window.addEventListener("popupshown", listener);
  });
  context_click(textbox);
  let contextPopup = await promise;

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(contextPopup, "popuphidden");
  contextPopup.hidePopup();
  await promise;

  textbox.value = "";
});

// Moving focus away from the search box should close the popup
add_task(async function focus_change_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let promise2 = promiseEvent(searchbar.textbox, "blur");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  await promise;
  await promise2;

  textbox.value = "";
});

// Moving focus away from the search box should close the small popup
add_task(async function focus_change_closes_small_popup() {
  gURLBar.focus();

  let promise = promiseEvent(searchPopup, "popupshown");
  // For some reason sending the mouse event immediately doesn't open the popup.
  SimpleTest.executeSoon(() => {
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  });
  await promise;
  is(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the small popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );

  promise = promiseEvent(searchPopup, "popuphidden");
  let promise2 = promiseEvent(searchbar.textbox, "blur");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  await promise;
  await promise2;
});

// Pressing escape should close the popup.
add_task(async function escape_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  textbox.value = "";
});

// Pressing contextmenu should close the popup.
add_task(async function contextmenu_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  context_click(textbox);
  await promise;

  let contextPopup = searchbar._menupopup;
  promise = promiseEvent(contextPopup, "popuphidden");
  contextPopup.hidePopup();
  await promise;

  textbox.value = "";
});

// Tabbing to the search box should open the popup if it contains text.
add_task(async function tab_opens_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_Tab");
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;

  textbox.value = "";
});

// Tabbing to the search box should not open the popup if it doesn't contain text.
add_no_popup_task(function tab_doesnt_open_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  EventUtils.synthesizeKey("KEY_Tab");

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  textbox.value = "";
});

// Switching back to the window when the search box has focus from mouse should not open the popup.
add_task(async function refocus_window_doesnt_open_popup_mouse() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(searchbar, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let newWin = OpenBrowserWindow();
  await new Promise(resolve => waitForFocus(resolve, newWin));
  await promise;

  function listener() {
    ok(false, "Should not have shown the popup.");
  }
  searchPopup.addEventListener("popupshowing", listener);

  promise = promiseEvent(searchbar.textbox, "focus");
  newWin.close();
  await promise;

  // Wait a few ticks to allow any focus handlers to show the popup if they are going to.
  await new Promise(resolve => executeSoon(resolve));
  await new Promise(resolve => executeSoon(resolve));
  await new Promise(resolve => executeSoon(resolve));

  searchPopup.removeEventListener("popupshowing", listener);
  textbox.value = "";
});

// Switching back to the window when the search box has focus from keyboard should not open the popup.
add_task(async function refocus_window_doesnt_open_popup_keyboard() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_Tab");
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let newWin = OpenBrowserWindow();
  await new Promise(resolve => waitForFocus(resolve, newWin));
  await promise;

  function listener() {
    ok(false, "Should not have shown the popup.");
  }
  searchPopup.addEventListener("popupshowing", listener);

  promise = promiseEvent(searchbar.textbox, "focus");
  newWin.close();
  await promise;

  // Wait a few ticks to allow any focus handlers to show the popup if they are going to.
  await new Promise(resolve => executeSoon(resolve));
  await new Promise(resolve => executeSoon(resolve));
  await new Promise(resolve => executeSoon(resolve));

  searchPopup.removeEventListener("popupshowing", listener);
  textbox.value = "";
});

// Clicking the search go button shouldn't open the popup
add_no_popup_task(async function search_go_doesnt_open_popup() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gURLBar.focus();
  textbox.value = "foo";
  searchbar.updateGoButtonVisibility();

  let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeMouseAtCenter(goButton, {});
  await promise;

  textbox.value = "";
  gBrowser.removeCurrentTab();
});

// Clicks outside the search popup should close the popup but not consume the click.
add_task(async function dont_consume_clicks() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  await EventUtils.promiseNativeMouseEventAndWaitForEvent({
    type: "click",
    target: gURLBar.inputField,
    atCenter: true,
    eventTypeToWait: "mouseup",
  });
  await promise;

  is(
    Services.focus.focusedElement,
    gURLBar.inputField,
    "Should have focused the URL bar"
  );

  textbox.value = "";
});

// Dropping text to the searchbar should open the popup
add_task(async function drop_opens_popup() {
  // The previous task leaves focus in the URL bar. However, in that case drags
  // can be interpreted as being selection drags by the drag manager, which
  // breaks the drag synthesis from EventUtils.js below. To avoid this, focus
  // the browser content instead.
  let focusEventPromise = BrowserTestUtils.waitForEvent(
    gBrowser.selectedBrowser,
    "focus"
  );
  gBrowser.selectedBrowser.focus();
  await focusEventPromise;

  let promise = promiseEvent(searchPopup, "popupshown");
  // Use a source for the drop that is outside of the search bar area, to avoid
  // it receiving a mousedown and causing the popup to sometimes open.
  let homeButton = document.getElementById("home-button");
  EventUtils.synthesizeDrop(
    homeButton,
    textbox,
    [[{ type: "text/plain", data: "foo" }]],
    "move",
    window
  );
  await promise;

  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );
  is(
    Services.focus.focusedElement,
    textbox,
    "Should have focused the search bar"
  );
  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;

  textbox.value = "";
});

// Moving the caret using the cursor keys should not close the popup.
add_task(async function dont_rollup_oncaretmove() {
  gURLBar.focus();
  textbox.value = "long text";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  await promise;

  // Deselect the text
  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(
    textbox.selectionStart,
    9,
    "Should have moved the caret (selectionStart after deselect right)"
  );
  is(
    textbox.selectionEnd,
    9,
    "Should have moved the caret (selectionEnd after deselect right)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(
    textbox.selectionStart,
    8,
    "Should have moved the caret (selectionStart after left)"
  );
  is(
    textbox.selectionEnd,
    8,
    "Should have moved the caret (selectionEnd after left)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(
    textbox.selectionStart,
    9,
    "Should have moved the caret (selectionStart after right)"
  );
  is(
    textbox.selectionEnd,
    9,
    "Should have moved the caret (selectionEnd after right)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  // Ensure caret movement works while a suggestion is selected.
  is(textbox.popup.selectedIndex, -1, "No selected item in list");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(textbox.popup.selectedIndex, 0, "Selected item in list");
  is(
    textbox.selectionStart,
    9,
    "Should have moved the caret to the end (selectionStart after selection)"
  );
  is(
    textbox.selectionEnd,
    9,
    "Should have moved the caret to the end (selectionEnd after selection)"
  );

  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(
    textbox.selectionStart,
    8,
    "Should have moved the caret again (selectionStart after left)"
  );
  is(
    textbox.selectionEnd,
    8,
    "Should have moved the caret again (selectionEnd after left)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(
    textbox.selectionStart,
    7,
    "Should have moved the caret (selectionStart after left)"
  );
  is(
    textbox.selectionEnd,
    7,
    "Should have moved the caret (selectionEnd after left)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(
    textbox.selectionStart,
    8,
    "Should have moved the caret (selectionStart after right)"
  );
  is(
    textbox.selectionEnd,
    8,
    "Should have moved the caret (selectionEnd after right)"
  );
  is(searchPopup.state, "open", "Popup should still be open");

  if (!navigator.platform.includes("Mac")) {
    EventUtils.synthesizeKey("KEY_Home");
    is(
      textbox.selectionStart,
      0,
      "Should have moved the caret (selectionStart after home)"
    );
    is(
      textbox.selectionEnd,
      0,
      "Should have moved the caret (selectionEnd after home)"
    );
    is(searchPopup.state, "open", "Popup should still be open");
  }

  // Close the popup again
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  textbox.value = "";
});

// Entering customization mode shouldn't open the popup.
add_task(async function dont_open_in_customization() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_Tab");
  await promise;
  isnot(
    searchPopup.getAttribute("showonlysettings"),
    "true",
    "Should show the full popup"
  );

  info("Entering customization mode");
  let sawPopup = false;
  function listener() {
    sawPopup = true;
  }
  searchPopup.addEventListener("popupshowing", listener);
  await gCUITestUtils.openMainMenu();
  promise = promiseEvent(searchPopup, "popuphidden");
  await startCustomizing();
  await promise;

  searchPopup.removeEventListener("popupshowing", listener);
  ok(!sawPopup, "Shouldn't have seen the suggestions popup");

  await endCustomizing();
  textbox.value = "";
});

add_task(async function cleanup() {
  info("removing search history values: " + kValues);
  let removeOps = kValues.map(value => {
    return { op: "remove", fieldname: "searchbar-history", value };
  });
  searchbar.FormHistory.update(removeOps);
});
