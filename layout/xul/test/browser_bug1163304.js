ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function() {
  BrowserSearch.searchBar.focus();

  let DOMWindowUtils = EventUtils._getDOMWindowUtils();
  is(DOMWindowUtils.IMEStatus, DOMWindowUtils.IME_STATUS_ENABLED,
     "IME should be available when searchbar has focus");

  let searchPopup = document.getElementById("PopupSearchAutoComplete");

  let shownPromise = BrowserTestUtils.waitForEvent(searchPopup, "popupshown");
  // Open popup of the searchbar
  EventUtils.synthesizeKey("KEY_F4");
  await shownPromise;
  await new Promise(r => setTimeout(r, 0));

  is(DOMWindowUtils.IMEStatus, DOMWindowUtils.IME_STATUS_ENABLED,
     "IME should be available even when the popup of searchbar is open");

  // Activate the menubar, then, the popup should be closed
  let hiddenPromise = BrowserTestUtils.waitForEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Alt");
  await hiddenPromise;
  await new Promise(r => setTimeout(r, 0));

  is(DOMWindowUtils.IMEStatus, DOMWindowUtils.IME_STATUS_DISABLED,
     "IME should not be available when menubar is active");
  // Inactivate the menubar (and restore the focus to the searchbar
  EventUtils.synthesizeKey("KEY_Escape");
  is(DOMWindowUtils.IMEStatus, DOMWindowUtils.IME_STATUS_ENABLED,
     "IME should be available after focus is back to the searchbar");
});
