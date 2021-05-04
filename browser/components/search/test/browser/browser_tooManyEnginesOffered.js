"use strict";

// This test makes sure that when a page offers many search engines, the search
// popup shows a submenu that lists them instead of showing them in the popup
// itself.

const searchPopup = document.getElementById("PopupSearchAutoComplete");

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();

  await Services.search.init();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function test() {
  let searchbar = BrowserSearch.searchBar;

  let rootDir = getRootDirectory(gTestPath);
  let url = rootDir + "tooManyEnginesOffered.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Open the search popup.
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  // In TV we may try opening too early, when the searchbar is not ready yet.
  await TestUtils.waitForCondition(
    () => BrowserSearch.searchBar.textbox.controller.input,
    "Wait for the searchbar controller to connect"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await promise;

  // Make sure it has only one add-engine menu button item.
  let items = getOpenSearchItems();
  Assert.equal(items.length, 1, "A single button");
  let menuButton = items[0];
  Assert.equal(menuButton.type, "menu", "A menu button");
  await document.l10n.translateElements([menuButton]);
  Assert.equal(menuButton.label, "Add search engine");

  // Mouse over the menu button to open it.
  let buttonPopup = menuButton.menupopup;
  promise = promiseEvent(buttonPopup, "popupshown");
  EventUtils.synthesizeMouse(menuButton, 5, 5, { type: "mousemove" });
  await promise;

  Assert.ok(menuButton.open, "Submenu should be open");

  // Check the engines inside the submenu.
  Assert.equal(buttonPopup.children.length, 6, "Expected number of engines");
  for (let i = 0; i < buttonPopup.children.length; i++) {
    let item = buttonPopup.children[i];
    Assert.equal(
      item.getAttribute("engine-name"),
      "engine" + (i + 1),
      "Expected engine title"
    );
  }

  // Mouse out of the menu button to close it.
  promise = promiseEvent(buttonPopup, "popuphidden");
  EventUtils.synthesizeMouse(searchbar, 5, 5, { type: "mousemove" });
  await promise;

  Assert.ok(!menuButton.open, "Submenu should be closed");

  // Key up until the menu button is selected.
  for (
    let button = null;
    button != menuButton;
    button = searchbar.textbox.popup.oneOffButtons.selectedButton
  ) {
    EventUtils.synthesizeKey("KEY_ArrowUp");
  }

  // Press the Right arrow key.  The submenu should open.
  promise = promiseEvent(buttonPopup, "popupshown");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  await promise;

  Assert.ok(menuButton.open, "Submenu should be open");

  // Press the Esc key.  The submenu should close.
  promise = promiseEvent(buttonPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  Assert.ok(!menuButton.open, "Submenu should be closed");

  gBrowser.removeCurrentTab();
});

function getOpenSearchItems() {
  let os = [];

  let addEngineList = searchPopup.oneOffButtons.addEngines;
  for (
    let item = addEngineList.firstElementChild;
    item;
    item = item.nextElementSibling
  ) {
    os.push(item);
  }

  return os;
}
