// Tests that the suggestion popup appears at the right times in response to
// focus and user events (mouse, keyboard, drop).

// Instead of loading EventUtils.js into the test scope in browser-test.js for all tests,
// we only need EventUtils.js for a few files which is why we are using loadSubScript.
var EventUtils = {};
this._scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

const searchbar = document.getElementById("searchbar");
const searchIcon = document.getAnonymousElementByAttribute(searchbar, "anonid", "searchbar-search-button");
const goButton = document.getAnonymousElementByAttribute(searchbar, "anonid", "search-go-button");
const textbox = searchbar._textbox;
const searchPopup = document.getElementById("PopupSearchAutoComplete");
const kValues = ["long text", "long text 2", "long text 3"];

const isWindows = Services.appinfo.OS == "WINNT";
const mouseDown = isWindows ? 2 : 1;
const mouseUp = isWindows ? 4 : 2;
const utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
const scale = utils.screenPixelsPerCSSPixel;

function* synthesizeNativeMouseClick(aElement) {
  let rect = aElement.getBoundingClientRect();
  let win = aElement.ownerGlobal;
  let x = win.mozInnerScreenX + (rect.left + rect.right) / 2;
  let y = win.mozInnerScreenY + (rect.top + rect.bottom) / 2;

  // Wait for the mouseup event to occur before continuing.
  return new Promise((resolve, reject) => {
    function eventOccurred(e)
    {
      aElement.removeEventListener("mouseup", eventOccurred, true);
      resolve();
    }

    aElement.addEventListener("mouseup", eventOccurred, true);

    utils.sendNativeMouseEvent(x * scale, y * scale, mouseDown, 0, null);
    utils.sendNativeMouseEvent(x * scale, y * scale, mouseUp, 0, null);
  });
}

add_task(function* init() {
  yield promiseNewEngine("testEngine.xml");

  // First cleanup the form history in case other tests left things there.
  yield new Promise((resolve, reject) => {
    info("cleanup the search history");
    searchbar.FormHistory.update({op: "remove", fieldname: "searchbar-history"},
                                 {handleCompletion: resolve,
                                  handleError: reject});
  });

  yield new Promise((resolve, reject) => {
    info("adding search history values: " + kValues);
    let ops = kValues.map(value => { return {op: "add",
                                             fieldname: "searchbar-history",
                                             value: value}
                                   });
    searchbar.FormHistory.update(ops, {
      handleCompletion: function() {
        registerCleanupFunction(() => {
          info("removing search history values: " + kValues);
          let ops =
            kValues.map(value => { return {op: "remove",
                                           fieldname: "searchbar-history",
                                           value: value}
                                 });
          searchbar.FormHistory.update(ops);
        });
        resolve();
      },
      handleError: reject
    });
  });
});

// Adds a task that shouldn't show the search suggestions popup.
function add_no_popup_task(task) {
  add_task(function*() {
    let sawPopup = false;
    function listener() {
      sawPopup = true;
    }

    info("Entering test " + task.name);
    searchPopup.addEventListener("popupshowing", listener, false);
    yield Task.spawn(task);
    searchPopup.removeEventListener("popupshowing", listener, false);
    ok(!sawPopup, "Shouldn't have seen the suggestions popup");
    info("Leaving test " + task.name);
  });
}

// Simulates the full set of events for a context click
function context_click(target) {
  for (let event of ["mousedown", "contextmenu", "mouseup"])
    EventUtils.synthesizeMouseAtCenter(target, { type: event, button: 2 });
}

// Right clicking the icon should not open the popup.
add_no_popup_task(function* open_icon_context() {
  gURLBar.focus();
  let toolbarPopup = document.getElementById("toolbar-context-menu");

  let promise = promiseEvent(toolbarPopup, "popupshown");
  context_click(searchIcon);
  yield promise;

  promise = promiseEvent(toolbarPopup, "popuphidden");
  toolbarPopup.hidePopup();
  yield promise;
});

// With no text in the search box left clicking the icon should open the popup.
// Clicking the icon again should hide the popup and not show it again.
add_task(function* open_empty() {
  gURLBar.focus();

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Clicking icon");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  yield promise;
  is(searchPopup.getAttribute("showonlysettings"), "true", "Should only show the settings");
  is(textbox.mController.searchString, "", "Should be an empty search string");

  // By giving the textbox some text any next attempt to open the search popup
  // from the click handler will try to search for this text.
  textbox.value = "foo";

  promise = promiseEvent(searchPopup, "popuphidden");
  let clickPromise = promiseEvent(searchIcon, "click");

  info("Hiding popup");
  yield synthesizeNativeMouseClick(searchIcon);
  yield promise;

  is(textbox.mController.searchString, "", "Should not have started to search for the new text");

  // Cancel the search if it started.
  if (textbox.mController.searchString != "") {
    textbox.mController.stopSearch();
  }

  textbox.value = "";
});

// With no text in the search box left clicking it should not open the popup.
add_no_popup_task(function* click_doesnt_open_popup() {
  gURLBar.focus();

  EventUtils.synthesizeMouseAtCenter(textbox, {});
  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 0, "Should have selected all of the text");
});

// Left clicking in a non-empty search box when unfocused should focus it and open the popup.
add_task(function* click_opens_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;

  textbox.value = "";
});

// Right clicking in a non-empty search box when unfocused should open the edit context menu.
add_no_popup_task(function* right_click_doesnt_open_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let contextPopup = document.getAnonymousElementByAttribute(textbox.inputField.parentNode, "anonid", "input-box-contextmenu");
  let promise = promiseEvent(contextPopup, "popupshown");
  context_click(textbox);
  yield promise;

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(contextPopup, "popuphidden");
  contextPopup.hidePopup();
  yield promise;

  textbox.value = "";
});

// Moving focus away from the search box should close the popup
add_task(function* focus_change_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let promise2 = promiseEvent(searchbar, "blur");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  yield promise;
  yield promise2;

  textbox.value = "";
});

// Moving focus away from the search box should close the small popup
add_task(function* focus_change_closes_small_popup() {
  gURLBar.focus();

  let promise = promiseEvent(searchPopup, "popupshown");
  // For some reason sending the mouse event immediately doesn't open the popup.
  SimpleTest.executeSoon(() => {
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  });
  yield promise;
  is(searchPopup.getAttribute("showonlysettings"), "true", "Should show the small popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");

  promise = promiseEvent(searchPopup, "popuphidden");
  let promise2 = promiseEvent(searchbar, "blur");
  EventUtils.synthesizeKey("VK_TAB", { shiftKey: true });
  yield promise;
  yield promise2;
});

// Pressing escape should close the popup.
add_task(function* escape_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  textbox.value = "";
});

// Pressing contextmenu should close the popup.
add_task(function* contextmenu_closes_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");

  // synthesizeKey does not work with VK_CONTEXT_MENU (bug 1127368)
  EventUtils.synthesizeMouseAtCenter(textbox, { type: "contextmenu", button: null });

  yield promise;

  let contextPopup =
    document.getAnonymousElementByAttribute(textbox.inputField.parentNode,
                                            "anonid", "input-box-contextmenu");
  promise = promiseEvent(contextPopup, "popuphidden");
  contextPopup.hidePopup();
  yield promise;

  textbox.value = "";
});

// Tabbing to the search box should open the popup if it contains text.
add_task(function* tab_opens_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("VK_TAB", {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;

  textbox.value = "";
});

// Tabbing to the search box should not open the popup if it doesn't contain text.
add_no_popup_task(function* tab_doesnt_open_popup() {
  gURLBar.focus();
  textbox.value = "foo";

  EventUtils.synthesizeKey("VK_TAB", {});

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  textbox.value = "";
});

// Switching back to the window when the search box has focus from mouse should not open the popup.
add_task(function* refocus_window_doesnt_open_popup_mouse() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(searchbar, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let newWin = OpenBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, newWin));
  yield promise;

  function listener() {
    ok(false, "Should not have shown the popup.");
  }
  searchPopup.addEventListener("popupshowing", listener, false);

  promise = promiseEvent(searchbar, "focus");
  newWin.close();
  yield promise;

  // Wait a few ticks to allow any focus handlers to show the popup if they are going to.
  yield new Promise(resolve => executeSoon(resolve));
  yield new Promise(resolve => executeSoon(resolve));
  yield new Promise(resolve => executeSoon(resolve));

  searchPopup.removeEventListener("popupshowing", listener, false);
  textbox.value = "";
});

// Switching back to the window when the search box has focus from keyboard should not open the popup.
add_task(function* refocus_window_doesnt_open_popup_keyboard() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("VK_TAB", {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  let newWin = OpenBrowserWindow();
  yield new Promise(resolve => waitForFocus(resolve, newWin));
  yield promise;

  function listener() {
    ok(false, "Should not have shown the popup.");
  }
  searchPopup.addEventListener("popupshowing", listener, false);

  promise = promiseEvent(searchbar, "focus");
  newWin.close();
  yield promise;

  // Wait a few ticks to allow any focus handlers to show the popup if they are going to.
  yield new Promise(resolve => executeSoon(resolve));
  yield new Promise(resolve => executeSoon(resolve));
  yield new Promise(resolve => executeSoon(resolve));

  searchPopup.removeEventListener("popupshowing", listener, false);
  textbox.value = "";
});

// Clicking the search go button shouldn't open the popup
add_no_popup_task(function* search_go_doesnt_open_popup() {
  gBrowser.selectedTab = gBrowser.addTab();

  gURLBar.focus();
  textbox.value = "foo";
  searchbar.updateGoButtonVisibility();

  let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeMouseAtCenter(goButton, {});
  yield promise;

  textbox.value = "";
  gBrowser.removeCurrentTab();
});

// Clicks outside the search popup should close the popup but not consume the click.
add_task(function* dont_consume_clicks() {
  gURLBar.focus();
  textbox.value = "foo";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;
  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");

  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  is(textbox.selectionStart, 0, "Should have selected all of the text");
  is(textbox.selectionEnd, 3, "Should have selected all of the text");

  promise = promiseEvent(searchPopup, "popuphidden");
  yield synthesizeNativeMouseClick(gURLBar);
  yield promise;

  is(Services.focus.focusedElement, gURLBar.inputField, "Should have focused the URL bar");

  textbox.value = "";
});

// Dropping text to the searchbar should open the popup
add_task(function* drop_opens_popup() {
  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeDrop(searchIcon, textbox.inputField, [[ {type: "text/plain", data: "foo" } ]], "move", window);
  yield promise;

  isnot(searchPopup.getAttribute("showonlysettings"), "true", "Should show the full popup");
  is(Services.focus.focusedElement, textbox.inputField, "Should have focused the search bar");
  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;

  textbox.value = "";
});

// Moving the caret using the cursor keys should not close the popup.
add_task(function* dont_rollup_oncaretmove() {
  gURLBar.focus();
  textbox.value = "long text";

  let promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(textbox, {});
  yield promise;

  // Deselect the text
  EventUtils.synthesizeKey("VK_RIGHT", {});
  is(textbox.selectionStart, 9, "Should have moved the caret (selectionStart after deselect right)");
  is(textbox.selectionEnd, 9, "Should have moved the caret (selectionEnd after deselect right)");
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("VK_LEFT", {});
  is(textbox.selectionStart, 8, "Should have moved the caret (selectionStart after left)");
  is(textbox.selectionEnd, 8, "Should have moved the caret (selectionEnd after left)");
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("VK_RIGHT", {});
  is(textbox.selectionStart, 9, "Should have moved the caret (selectionStart after right)");
  is(textbox.selectionEnd, 9, "Should have moved the caret (selectionEnd after right)");
  is(searchPopup.state, "open", "Popup should still be open");

  // Ensure caret movement works while a suggestion is selected.
  is(textbox.popup.selectedIndex, -1, "No selected item in list");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(textbox.popup.selectedIndex, 0, "Selected item in list");
  is(textbox.selectionStart, 9, "Should have moved the caret to the end (selectionStart after selection)");
  is(textbox.selectionEnd, 9, "Should have moved the caret to the end (selectionEnd after selection)");

  EventUtils.synthesizeKey("VK_LEFT", {});
  is(textbox.selectionStart, 8, "Should have moved the caret again (selectionStart after left)");
  is(textbox.selectionEnd, 8, "Should have moved the caret again (selectionEnd after left)");
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("VK_LEFT", {});
  is(textbox.selectionStart, 7, "Should have moved the caret (selectionStart after left)");
  is(textbox.selectionEnd, 7, "Should have moved the caret (selectionEnd after left)");
  is(searchPopup.state, "open", "Popup should still be open");

  EventUtils.synthesizeKey("VK_RIGHT", {});
  is(textbox.selectionStart, 8, "Should have moved the caret (selectionStart after right)");
  is(textbox.selectionEnd, 8, "Should have moved the caret (selectionEnd after right)");
  is(searchPopup.state, "open", "Popup should still be open");

  if (navigator.platform.indexOf("Mac") == -1) {
    EventUtils.synthesizeKey("VK_HOME", {});
    is(textbox.selectionStart, 0, "Should have moved the caret (selectionStart after home)");
    is(textbox.selectionEnd, 0, "Should have moved the caret (selectionEnd after home)");
    is(searchPopup.state, "open", "Popup should still be open");
  }

  // Close the popup again
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  textbox.value = "";
});
