// Tests that keyboard navigation in the search panel works as designed.

const searchPopup = document.getElementById("PopupSearchAutoComplete");

const kValues = ["foo1", "foo2", "foo3"];
const kUserValue = "foo";

function getOpenSearchItems() {
  let os = [];

  let addEngineList = searchPopup.searchOneOffsContainer.querySelector(
    ".search-add-engines"
  );
  for (
    let item = addEngineList.firstElementChild;
    item;
    item = item.nextElementSibling
  ) {
    os.push(item);
  }

  return os;
}

let searchbar;
let textbox;

async function checkHeader(engine) {
  // The header can be updated after getting the engine, so we may have to
  // wait for it.
  let header = searchPopup.searchbarEngineName;
  if (!header.getAttribute("value").includes(engine.name)) {
    await new Promise(resolve => {
      let observer = new MutationObserver(() => {
        observer.disconnect();
        resolve();
      });
      observer.observe(searchPopup.searchbarEngineName, {
        attributes: true,
        attributeFilter: ["value"],
      });
    });
  }
  Assert.ok(
    header.getAttribute("value").includes(engine.name),
    "Should have the correct engine name displayed in the header"
  );
}

add_setup(async function () {
  searchbar = await gCUITestUtils.addSearchBar();
  textbox = searchbar.textbox;

  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "testEngine.xml",
    setAsDefault: true,
  });
  // First cleanup the form history in case other tests left things there.
  info("cleanup the search history");
  await FormHistory.update({ op: "remove", fieldname: "searchbar-history" });

  info("adding search history values: " + kValues);
  let addOps = kValues.map(value => {
    return { op: "add", fieldname: "searchbar-history", value };
  });
  await FormHistory.update(addOps);

  textbox.value = kUserValue;

  registerCleanupFunction(async () => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test_arrows() {
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  await promise;
  is(
    textbox.mController.searchString,
    kUserValue,
    "The search string should be 'foo'"
  );

  // Check the initial state of the panel before sending keyboard events.
  is(searchPopup.matchCount, kValues.length, "There should be 3 suggestions");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // The tests will be less meaningful if the first, second, last, and
  // before-last one-off buttons aren't different. We should always have more
  // than 4 default engines, but it's safer to check this assumption.
  let oneOffs = getOneOffs();
  ok(oneOffs.length >= 4, "we have at least 4 one-off buttons displayed");

  ok(!textbox.selectedButton, "no one-off button should be selected");

  // The down arrow should first go through the suggestions.
  for (let i = 0; i < kValues.length; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    is(
      searchPopup.selectedIndex,
      i,
      "the suggestion at index " + i + " should be selected"
    );
    is(
      textbox.value,
      kValues[i],
      "the textfield value should be " + kValues[i]
    );
    await checkHeader(Services.search.defaultEngine);
  }

  // Pressing down again should remove suggestion selection and change the text
  // field value back to what the user typed, and select the first one-off.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(
    textbox.value,
    kUserValue,
    "the textfield value should be back to initial value"
  );

  // now cycle through the one-off items, the first one is already selected.
  for (let i = 0; i < oneOffs.length; ++i) {
    let oneOffButton = oneOffs[i];
    is(
      textbox.selectedButton,
      oneOffButton,
      "the one-off button #" + (i + 1) + " should be selected"
    );
    await checkHeader(oneOffButton.engine);
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }

  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );
  await checkHeader(Services.search.defaultEngine);
  EventUtils.synthesizeKey("KEY_ArrowDown");

  // We should now be back to the initial situation.
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  ok(!textbox.selectedButton, "no one-off button should be selected");
  await checkHeader(Services.search.defaultEngine);

  info("now test the up arrow key");
  EventUtils.synthesizeKey("KEY_ArrowUp");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );
  await checkHeader(Services.search.defaultEngine);

  // cycle through the one-off items, the first one is already selected.
  for (let i = oneOffs.length; i; --i) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    let oneOffButton = oneOffs[i - 1];
    is(
      textbox.selectedButton,
      oneOffButton,
      "the one-off button #" + i + " should be selected"
    );
    await checkHeader(oneOffButton.engine);
  }

  // Another press on up should clear the one-off selection and select the
  // last suggestion.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  ok(!textbox.selectedButton, "no one-off button should be selected");

  for (let i = kValues.length - 1; i >= 0; --i) {
    is(
      searchPopup.selectedIndex,
      i,
      "the suggestion at index " + i + " should be selected"
    );
    is(
      textbox.value,
      kValues[i],
      "the textfield value should be " + kValues[i]
    );
    await checkHeader(Services.search.defaultEngine);
    EventUtils.synthesizeKey("KEY_ArrowUp");
  }

  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(
    textbox.value,
    kUserValue,
    "the textfield value should be back to initial value"
  );
});

add_task(async function test_typing_clears_button_selection() {
  is(
    Services.focus.focusedElement,
    textbox,
    "the search bar should be focused"
  ); // from the previous test.
  ok(!textbox.selectedButton, "no button should be selected");

  EventUtils.synthesizeKey("KEY_ArrowUp");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );

  // Type a character.
  EventUtils.sendString("a");
  ok(!textbox.selectedButton, "the settings item should be de-selected");

  // Remove the character.
  EventUtils.synthesizeKey("KEY_Backspace");
});

add_task(async function test_tab() {
  is(
    Services.focus.focusedElement,
    textbox,
    "the search bar should be focused"
  ); // from the previous test.

  let oneOffs = getOneOffs();
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // Pressing tab should select the first one-off without selecting suggestions.
  // now cycle through the one-off items, the first one is already selected.
  for (let i = 0; i < oneOffs.length; ++i) {
    EventUtils.synthesizeKey("KEY_Tab");
    is(
      textbox.selectedButton,
      oneOffs[i],
      "the one-off button #" + (i + 1) + " should be selected"
    );
  }
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // One more <tab> selects the settings button.
  EventUtils.synthesizeKey("KEY_Tab");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );

  // Pressing tab again should close the panel...
  let promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Tab");
  await promise;

  // ... and move the focus out of the searchbox.
  ok(
    !Services.focus.focusedElement.classList.contains("searchbar-textbox"),
    "the search input in the search bar should no longer be focused"
  );
});

add_task(async function test_shift_tab() {
  // First reopen the panel.
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  await promise;

  let oneOffs = getOneOffs();
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // Press up once to select the last button.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );

  // Press up again to select the last one-off button.
  EventUtils.synthesizeKey("KEY_ArrowUp");

  // Pressing shift+tab should cycle through the one-off items.
  for (let i = oneOffs.length - 1; i >= 0; --i) {
    is(
      textbox.selectedButton,
      oneOffs[i],
      "the one-off button #" + (i + 1) + " should be selected"
    );
    if (i) {
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
    }
  }
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing shift+tab again should close the panel...
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  await promise;

  // ... and move the focus out of the searchbox.
  ok(
    !Services.focus.focusedElement.classList.contains("searchbar-textbox"),
    "the search input in the search bar should no longer be focused"
  );

  // Return the focus to the search bar
  EventUtils.synthesizeKey("KEY_Tab");
  ok(
    Services.focus.focusedElement.classList.contains("searchbar-textbox"),
    "the search bar should be focused"
  );

  // ... and confirm the input value was autoselected and is replaced.
  EventUtils.synthesizeKey("fo");
  is(
    Services.focus.focusedElement.value,
    "fo",
    "when the search bar was focused, the value should be autoselected"
  );
  // Return to the expected value
  EventUtils.synthesizeKey("o");
});

add_task(async function test_alt_down() {
  // First refocus the panel.
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  await promise;

  // close the panel using the escape key.
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  // check that alt+down opens the panel...
  promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  await promise;

  // ... and does nothing else.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing alt+down should select the first one-off without selecting suggestions
  // and cycle through the one-off items.
  let oneOffs = getOneOffs();
  for (let i = 0; i < oneOffs.length; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    is(
      textbox.selectedButton,
      oneOffs[i],
      "the one-off button #" + (i + 1) + " should be selected"
    );
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }

  // One more alt+down keypress and nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // another one and the first one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  is(
    textbox.selectedButton,
    oneOffs[0],
    "the first one-off button should be selected"
  );
});

add_task(async function test_alt_up() {
  // close the panel using the escape key.
  let promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;
  ok(
    !textbox.selectedButton,
    "no one-off button should be selected after closing the panel"
  );

  // check that alt+up opens the panel...
  promise = promiseEvent(searchPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  await promise;

  // ... and does nothing else.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // Pressing alt+up should select the last one-off without selecting suggestions
  // and cycle up through the one-off items.
  let oneOffs = getOneOffs();
  for (let i = oneOffs.length - 1; i >= 0; --i) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
    is(
      textbox.selectedButton,
      oneOffs[i],
      "the one-off button #" + (i + 1) + " should be selected"
    );
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }

  // One more alt+down keypress and nothing should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // another one and the last one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowUp", { altKey: true });
  is(
    textbox.selectedButton,
    oneOffs[oneOffs.length - 1],
    "the last one-off button should be selected"
  );

  // Cleanup for the next test.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(!textbox.selectedButton, "no one-off should be selected anymore");
});

add_task(async function test_accel_down() {
  // Pressing accel+down should select the next visible search engine, without
  // selecting suggestions.
  let engines = await Services.search.getVisibleEngines();
  let current = Services.search.defaultEngine;
  let currIdx = -1;
  for (let i = 0, l = engines.length; i < l; ++i) {
    if (engines[i].name == current.name) {
      currIdx = i;
      break;
    }
  }
  for (let i = 0, l = engines.length; i < l; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown", { accelKey: true });
    await SearchTestUtils.promiseSearchNotification(
      "engine-default",
      "browser-search-engine-modified"
    );
    let expected = engines[++currIdx % engines.length];
    is(
      Services.search.defaultEngine.name,
      expected.name,
      "Default engine should have changed"
    );
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }
  Services.search.defaultEngine = current;
});

add_task(async function test_accel_up() {
  // Pressing accel+down should select the previous visible search engine, without
  // selecting suggestions.
  let engines = await Services.search.getVisibleEngines();
  let current = Services.search.defaultEngine;
  let currIdx = -1;
  for (let i = 0, l = engines.length; i < l; ++i) {
    if (engines[i].name == current.name) {
      currIdx = i;
      break;
    }
  }
  for (let i = 0, l = engines.length; i < l; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowUp", { accelKey: true });
    await SearchTestUtils.promiseSearchNotification(
      "engine-default",
      "browser-search-engine-modified"
    );
    let expected =
      engines[--currIdx < 0 ? (currIdx = engines.length - 1) : currIdx];
    is(
      Services.search.defaultEngine.name,
      expected.name,
      "Default engine should have changed"
    );
    is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  }
  Services.search.defaultEngine = current;
});

add_task(async function test_tab_and_arrows() {
  // Check the initial state is as expected.
  ok(!textbox.selectedButton, "no one-off button should be selected");
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  is(textbox.value, kUserValue, "the textfield value should be unmodified");

  // After pressing down, the first sugggestion should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(searchPopup.selectedIndex, 0, "first suggestion should be selected");
  is(textbox.value, kValues[0], "the textfield value should have changed");
  ok(!textbox.selectedButton, "no one-off button should be selected");

  // After pressing tab, the first one-off should be selected,
  // and no suggestion should be selected.
  let oneOffs = getOneOffs();
  EventUtils.synthesizeKey("KEY_Tab");
  is(
    textbox.selectedButton,
    oneOffs[0],
    "the first one-off button should be selected"
  );
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // After pressing down, the second one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    textbox.selectedButton,
    oneOffs[1],
    "the second one-off button should be selected"
  );
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // After pressing right, the third one-off should be selected.
  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(
    textbox.selectedButton,
    oneOffs[2],
    "the third one-off button should be selected"
  );
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // After pressing left, the second one-off should be selected again.
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  is(
    textbox.selectedButton,
    oneOffs[1],
    "the second one-off button should be selected again"
  );
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // After pressing up, the first one-off should be selected again.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    textbox.selectedButton,
    oneOffs[0],
    "the first one-off button should be selected again"
  );
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");

  // After pressing up again, the last suggestion should be selected.
  // the textfield value back to the user-typed value, and still the first one-off
  // selected.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  is(
    searchPopup.selectedIndex,
    kValues.length - 1,
    "last suggestion should be selected"
  );
  is(
    textbox.value,
    kValues[kValues.length - 1],
    "the textfield value should match the suggestion"
  );
  is(textbox.selectedButton, null, "no one-off button should be selected");

  // Now pressing down should select the first one-off.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  is(
    textbox.selectedButton,
    oneOffs[0],
    "the first one-off button should be selected"
  );
  is(searchPopup.selectedIndex, -1, "there should be no selected suggestion");

  // Finally close the panel.
  let promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;
});

add_task(async function test_open_search() {
  let rootDir = getRootDirectory(gTestPath);
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    rootDir + "opensearch.html"
  );

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  await promise;

  let engines = searchPopup.querySelectorAll(
    ".searchbar-engine-one-off-add-engine"
  );
  is(engines.length, 3, "the opensearch.html page exposes 3 engines");

  // Check that there's initially no selection.
  is(searchPopup.selectedIndex, -1, "no suggestion should be selected");
  ok(!textbox.selectedButton, "no button should be selected");

  // Pressing up once selects the setting button...
  EventUtils.synthesizeKey("KEY_ArrowUp");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );

  // ...and then pressing up selects open search engines.
  for (let i = engines.length; i; --i) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
    let selectedButton = textbox.selectedButton;
    is(
      selectedButton,
      engines[i - 1],
      "the engine #" + i + " should be selected"
    );
    ok(
      selectedButton.classList.contains("searchbar-engine-one-off-add-engine"),
      "the button is themed as an add engine"
    );
  }

  // Pressing up again should select the last one-off button.
  EventUtils.synthesizeKey("KEY_ArrowUp");
  const allOneOffs = getOneOffs();
  is(
    textbox.selectedButton,
    allOneOffs[allOneOffs.length - engines.length - 1],
    "the last one-off button should be selected"
  );

  info("now check that the down key navigates open search items as expected");
  for (let i = 0; i < engines.length; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    is(
      textbox.selectedButton,
      engines[i],
      "the engine #" + (i + 1) + " should be selected"
    );
  }

  // Pressing down on the last engine item selects the settings button.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(
    textbox.selectedButton.classList.contains("search-setting-button"),
    "the settings item should be selected"
  );

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;

  gBrowser.removeCurrentTab();
});

add_task(async function cleanup() {
  info("removing search history values: " + kValues);
  let removeOps = kValues.map(value => {
    return { op: "remove", fieldname: "searchbar-history", value };
  });
  await FormHistory.update(removeOps);

  textbox.value = "";
});
