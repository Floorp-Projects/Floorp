"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// eslint-disable-next-line camelcase
add_task(async function test_searchbar_a11y_tree() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", true]],
  });

  // This used to rely on the implied 100ms initial timer of
  // TestUtils.waitForCondition. See bug 1700735.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 100));
  let searchbar = await TestUtils.waitForCondition(
    () => document.getElementById("searchbar"),
    "wait for search bar to appear"
  );

  // Make sure the popup has been rendered so it shows up in the a11y tree.
  let popup = document.getElementById("PopupSearchAutoComplete");
  let promise = Promise.all([
    BrowserTestUtils.waitForEvent(popup, "popupshown", false),
    waitForEvent(EVENT_SHOW, popup),
  ]);
  searchbar.textbox.openPopup();
  await promise;

  let TREE = {
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

  promise = Promise.all([
    BrowserTestUtils.waitForEvent(popup, "popuphidden", false),
    waitForEvent(EVENT_HIDE, popup),
  ]);
  searchbar.textbox.closePopup();
  await promise;

  TREE = {
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

      // the result list should be removed from the tree on popuphidden
    ],
  };

  testAccessibleTree(searchbar, TREE);
});
