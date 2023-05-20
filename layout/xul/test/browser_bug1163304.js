const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function () {
  const promiseFocusInSearchBar = BrowserTestUtils.waitForEvent(
    BrowserSearch.searchBar.textbox,
    "focus"
  );
  BrowserSearch.searchBar.focus();
  await promiseFocusInSearchBar;

  let DOMWindowUtils = EventUtils._getDOMWindowUtils();
  is(
    DOMWindowUtils.IMEStatus,
    DOMWindowUtils.IME_STATUS_ENABLED,
    "IME should be available when searchbar has focus"
  );

  let searchPopup = document.getElementById("PopupSearchAutoComplete");

  // Open popup of the searchbar
  // Oddly, F4 key press is sometimes not handled by the search bar.
  // It's out of scope of this test, so, let's retry to open it if failed.
  await (async () => {
    async function tryToOpen() {
      try {
        BrowserSearch.searchBar.focus();
        EventUtils.synthesizeKey("KEY_F4");
        await TestUtils.waitForCondition(
          () => searchPopup.state == "open",
          "The popup isn't opened",
          5,
          100
        );
      } catch (e) {
        // timed out, let's just return false without asserting the failure.
        return false;
      }
      return true;
    }
    for (let i = 0; i < 5; i++) {
      if (await tryToOpen()) {
        return;
      }
    }
    ok(false, "Failed to open the popup of searchbar");
  })();

  is(
    DOMWindowUtils.IMEStatus,
    DOMWindowUtils.IME_STATUS_ENABLED,
    "IME should be available even when the popup of searchbar is open"
  );

  // Activate the menubar, then, the popup should be closed
  is(searchPopup.state, "open", "The popup of searchbar shouldn't be closed");
  let hiddenPromise = BrowserTestUtils.waitForEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Alt");
  await hiddenPromise;
  await new Promise(r => setTimeout(r, 0));

  is(
    DOMWindowUtils.IMEStatus,
    DOMWindowUtils.IME_STATUS_DISABLED,
    "IME should not be available when menubar is active"
  );
  // Inactivate the menubar (and restore the focus to the searchbar
  EventUtils.synthesizeKey("KEY_Escape");
  is(
    DOMWindowUtils.IMEStatus,
    DOMWindowUtils.IME_STATUS_ENABLED,
    "IME should be available after focus is back to the searchbar"
  );
});
