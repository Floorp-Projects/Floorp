"use strict";

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.search", true]],
  });
});

add_task(async function test_show_search_term_tooltip_in_subdialog() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  let keyword = "organization";
  await runSearchInput(keyword);

  let formAutofillGroupBox = gBrowser.contentDocument.getElementById(
    "formAutofillGroupBox"
  );
  let savedAddressesButton =
    formAutofillGroupBox.querySelector(".accessory-button");

  info("Clicking saved addresses button to open subdialog");
  savedAddressesButton.click();
  info("Waiting for addresses subdialog to appear");
  await BrowserTestUtils.waitForCondition(() => {
    let dialogBox = gBrowser.contentDocument.querySelector(".dialogBox");
    return !!dialogBox;
  });
  let tooltip = gBrowser.contentDocument.querySelector(".search-tooltip");

  is_element_visible(
    tooltip,
    "Tooltip with search term should be visible in subdialog"
  );
  is(tooltip.textContent, keyword, "Tooltip should have correct search term");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
