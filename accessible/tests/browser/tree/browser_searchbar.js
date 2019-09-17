"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// eslint-disable-next-line camelcase
add_task(async function test_searchbar_a11y_tree() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", true]],
  });

  let searchbar = await TestUtils.waitForCondition(
    () => document.getElementById("searchbar"),
    "wait for search bar to appear"
  );

  // Make sure the popup has been rendered so it shows up in the a11y tree.
  let popup = document.getElementById("PopupSearchAutoComplete");
  let promise = BrowserTestUtils.waitForEvent(popup, "popupshown", false);
  searchbar.textbox.openPopup();
  await promise;

  promise = BrowserTestUtils.waitForEvent(popup, "popuphidden", false);
  searchbar.textbox.closePopup();
  await promise;

  const TREE = {
    role: ROLE_EDITCOMBOBOX,

    children: [
      // input element
      {
        role: ROLE_ENTRY,
        children: [],
      },

      // context menu
      {
        role: ROLE_COMBOBOX_LIST,
        children: [],
      },

      // result list
      {
        role: ROLE_GROUPING,
        // not testing the structure inside the result list
      },
    ],
  };

  testAccessibleTree(searchbar, TREE);
});
