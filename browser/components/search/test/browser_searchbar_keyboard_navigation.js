// Tests that keyboard navigation in the search panel works as designed.

const searchbar = document.getElementById("searchbar");
const textbox = searchbar._textbox;
const searchPopup = document.getElementById("PopupSearchAutoComplete");

const kValues = ["foo1", "foo2", "foo3"];
const kUserValue = "foo";

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let oneOff = document.getAnonymousElementByAttribute(searchPopup, "anonid",
                                                       "search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.classList.contains("dummy"))
      break;
    oneOffs.push(oneOff);
  }

  return oneOffs;
}

function getOpenSearchItems() {
  let os = [];

  let addEngineList =
    document.getAnonymousElementByAttribute(searchPopup, "anonid",
                                            "add-engines");
  for (let item = addEngineList.firstChild; item; item = item.nextSibling)
    os.push(item);

  return os;
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

  textbox.value = kUserValue;
  registerCleanupFunction(() => { textbox.value = ""; });
});


add_task(function* test_arrows() {
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  yield promise;
  is(textbox.mController.searchString, kUserValue, "The search string should be 'foo'");

  // Check the initial state of the panel before sending keyboard events.
  is(searchPopup.view.rowCount, kValues.length, "There should be 3 suggestions");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // The tests will be less meaningful if the first, second, last, and
  // before-last one-off buttons aren't different. We should always have more
  // than 4 default engines, but it's safer to check this assumption.
  let oneOffs = getOneOffs();
  ok(oneOffs.length >= 4, "we have at least 4 one-off buttons displayed")

  ok(!textbox.selectedButton, "no one-off button should be selected");

  // The down arrow should first go through the suggestions.
  for (let i = 0; i < kValues.length; ++i) {
    EventUtils.synthesizeKey("VK_DOWN", {});
    is(searchPopup.selectedIndex, i,
       "the suggestion at index " + i + " should be selected");
    is(textbox.value, kValues[i],
       "the textfield value should be " + kValues[i]);
  }

  // Pressing down again should remove suggestion selection and change the text
  // field value back to what the user typed, and select the first one-off.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue,
     "the textfield value should be back to initial value");

  // now cycle through the one-off items, the first one is already selected.
  for (let i = 0; i < oneOffs.length; ++i) {
    is(textbox.selectedButton, oneOffs[i],
       "the one-off button #" + (i + 1) + " should be selected");
    EventUtils.synthesizeKey("VK_DOWN", {});
  }

  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");
  EventUtils.synthesizeKey("VK_DOWN", {});

  // We should now be back to the initial situation.
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  ok(!textbox.selectedButton, "no one-off button should be selected");

  info("now test the up arrow key");
  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  // cycle through the one-off items, the first one is already selected.
  for (let i = oneOffs.length; i; --i) {
    EventUtils.synthesizeKey("VK_UP", {});
    is(textbox.selectedButton, oneOffs[i - 1],
       "the one-off button #" + i + " should be selected");
  }

  // Another press on up should clear the one-off selection and select the
  // last suggestion.
  EventUtils.synthesizeKey("VK_UP", {});
  ok(!textbox.selectedButton, "no one-off button should be selected");

  for (let i = kValues.length - 1; i >= 0; --i) {
    is(searchPopup.selectedIndex, i,
       "the suggestion at index " + i + " should be selected");
    is(textbox.value, kValues[i],
       "the textfield value should be " + kValues[i]);
    EventUtils.synthesizeKey("VK_UP", {});
  }

  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue,
     "the textfield value should be back to initial value");
});

add_task(function* test_typing_clears_button_selection() {
  is(Services.focus.focusedElement, textbox.inputField,
     "the search bar should be focused"); // from the previous test.
  ok(!textbox.selectedButton, "no button should be selected");

  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  // Type a character.
  EventUtils.synthesizeKey("a", {});
  ok(!textbox.selectedButton, "the settings item should be de-selected");

  // Remove the character.
  EventUtils.synthesizeKey("VK_BACK_SPACE", {});
});

add_task(function* test_tab() {
  is(Services.focus.focusedElement, textbox.inputField,
     "the search bar should be focused"); // from the previous test.

  let oneOffs = getOneOffs();
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // Pressing tab should select the first one-off without selecting suggestions.
  // now cycle through the one-off items, the first one is already selected.
  for (let i = 0; i < oneOffs.length; ++i) {
    EventUtils.synthesizeKey("VK_TAB", {});
    is(textbox.selectedButton, oneOffs[i],
       "the one-off button #" + (i + 1) + " should be selected");
  }
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // One more <tab> selects the settings button.
  EventUtils.synthesizeKey("VK_TAB", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  // Pressing tab again should close the panel...
  let promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_TAB", {});
  yield promise;

  // ... and move the focus out of the searchbox.
  isnot(Services.focus.focusedElement, textbox.inputField,
        "the search bar no longer be focused");
});

add_task(function* test_shift_tab() {
  // First reopen the panel.
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  yield promise;

  let oneOffs = getOneOffs();
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // Press up once to select the last button.
  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  // Press up again to select the last one-off button.
  EventUtils.synthesizeKey("VK_UP", {});

  // Pressing shift+tab should cycle through the one-off items.
  for (let i = oneOffs.length - 1; i >= 0; --i) {
    is(textbox.selectedButton, oneOffs[i],
       "the one-off button #" + (i + 1) + " should be selected");
    if (i)
      EventUtils.synthesizeKey("VK_TAB", {shiftKey: true});
  }
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing shift+tab again should close the panel...
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true});
  yield promise;

  // ... and move the focus out of the searchbox.
  isnot(Services.focus.focusedElement, textbox.inputField,
        "the search bar no longer be focused");
});

add_task(function* test_alt_down() {
  // First refocus the panel.
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  yield promise;

  // close the panel using the escape key.
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  // check that alt+down opens the panel...
  promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("VK_DOWN", {altKey: true});
  yield promise;

  // ... and does nothing else.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing alt+down should select the first one-off without selecting suggestions
  // and cycle through the one-off items.
  let oneOffs = getOneOffs();
  for (let i = 0; i < oneOffs.length; ++i) {
    EventUtils.synthesizeKey("VK_DOWN", {altKey: true});
    is(textbox.selectedButton, oneOffs[i],
       "the one-off button #" + (i + 1) + " should be selected");
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }

  // One more alt+down keypress and nothing should be selected.
  EventUtils.synthesizeKey("VK_DOWN", {altKey: true});
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // another one and the first one-off should be selected.
  EventUtils.synthesizeKey("VK_DOWN", {altKey: true});
  is(textbox.selectedButton, oneOffs[0],
     "the first one-off button should be selected");
});

add_task(function* test_alt_up() {
  // close the panel using the escape key.
  let promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;

  // check that alt+up opens the panel...
  promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("VK_UP", {altKey: true});
  yield promise;

  // ... and does nothing else.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing alt+up should select the last one-off without selecting suggestions
  // and cycle up through the one-off items.
  let oneOffs = getOneOffs();
  for (let i = oneOffs.length - 1; i >= 0; --i) {
    EventUtils.synthesizeKey("VK_UP", {altKey: true});
    is(textbox.selectedButton, oneOffs[i],
       "the one-off button #" + (i + 1) + " should be selected");
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }

  // One more alt+down keypress and nothing should be selected.
  EventUtils.synthesizeKey("VK_UP", {altKey: true});
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // another one and the last one-off should be selected.
  EventUtils.synthesizeKey("VK_UP", {altKey: true});
  is(textbox.selectedButton, oneOffs[oneOffs.length - 1],
     "the last one-off button should be selected");

  // Cleanup for the next test.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");
  EventUtils.synthesizeKey("VK_DOWN", {});
  ok(!textbox.selectedButton, "no one-off should be selected anymore");
});

add_task(function* test_tab_and_arrows() {
  // Check the initial state is as expected.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // After pressing down, the first sugggestion should be selected.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(searchPopup.selectedIndex, 0, "first suggestion should be selected");
  is(textbox.value, kValues[0], "the textfield value should have changed");
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // After pressing tab, the first one-off should be selected,
  // and the first suggestion still selected.
  let oneOffs = getOneOffs();
  EventUtils.synthesizeKey("VK_TAB", {});
  is(textbox.selectedButton, oneOffs[0],
     "the first one-off button should be selected");
  is(searchPopup.selectedIndex, 0, "first suggestion should still be selected");

  // After pressing down, the second suggestion should be selected,
  // and the first one-off still selected.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(textbox.selectedButton, oneOffs[0],
     "the first one-off button should still be selected");
  is(searchPopup.selectedIndex, 1, "second suggestion should be selected");

  // After pressing up, the first suggestion should be selected again,
  // and the first one-off still selected.
  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton, oneOffs[0],
     "the first one-off button should still be selected");
  is(searchPopup.selectedIndex, 0, "second suggestion should be selected again");

  // After pressing up again, we should have no suggestion selected anymore,
  // the textfield value back to the user-typed value, and still the first one-off
  // selected.
  EventUtils.synthesizeKey("VK_UP", {});
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue,
     "the textfield value should be back to user typed value");
  is(textbox.selectedButton, oneOffs[0],
     "the first one-off button should still be selected");

  // Now pressing down should select the second one-off.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(textbox.selectedButton, oneOffs[1],
     "the second one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "there should still be no selected suggestion");

  // Finally close the panel.
  let promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;
});

add_task(function* test_open_search() {
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  let deferred = Promise.defer();
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);
    deferred.resolve();
  }, true);

  let rootDir = getRootDirectory(gTestPath);
  content.location = rootDir + "opensearch.html";

  yield deferred.promise;

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  yield promise;

  let engines = getOpenSearchItems();
  is(engines.length, 2, "the opensearch.html page exposes 2 engines")

  // Check that there's initially no selection.
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  ok(!textbox.selectedButton, "no button should be selected");

  // Pressing up once selects the setting button...
  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  // ...and then pressing up selects open search engines.
  for (let i = engines.length; i; --i) {
    EventUtils.synthesizeKey("VK_UP", {});
    let selectedButton = textbox.selectedButton;
    is(selectedButton, engines[i - 1],
       "the engine #" + i + " should be selected");
    ok(selectedButton.classList.contains("addengine-item"),
       "the button is themed as an engine item");
  }

  // Pressing up again should select the last one-off button.
  EventUtils.synthesizeKey("VK_UP", {});
  is(textbox.selectedButton, getOneOffs().pop(),
     "the last one-off button should be selected");

  info("now check that the down key navigates open search items as expected");
  for (let i = 0; i < engines.length; ++i) {
    EventUtils.synthesizeKey("VK_DOWN", {});
    is(textbox.selectedButton, engines[i],
       "the engine #" + (i + 1) + " should be selected");
  }

  // Pressing down on the last engine item selects the settings button.
  EventUtils.synthesizeKey("VK_DOWN", {});
  is(textbox.selectedButton.getAttribute("anonid"), "search-settings",
     "the settings item should be selected");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;

  gBrowser.removeCurrentTab();
});
