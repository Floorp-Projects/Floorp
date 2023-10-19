/**
 * Test the Tab Manager visibility respects browser.tabs.tabmanager.enabled preference
 *  */

"use strict";

// The hostname for the test URIs.
const TEST_HOSTNAME = "https://example.com";
const DUMMY_PAGE_PATH = "/browser/base/content/test/tabs/dummy_page.html";

add_task(async function tab_manager_visibility_preference_on() {
  Services.prefs.setBoolPref("browser.tabs.tabmanager.enabled", true);

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: newWindow.gBrowser,
      url: TEST_HOSTNAME + DUMMY_PAGE_PATH,
    },
    async function (browser) {
      await Assert.ok(
        BrowserTestUtils.is_visible(
          newWindow.document.getElementById("alltabs-button")
        ),
        "tab manage menu is visible when browser.tabs.tabmanager.enabled preference is set to true"
      );
    }
  );
  Services.prefs.clearUserPref("browser.tabs.tabmanager.enabled");
  BrowserTestUtils.closeWindow(newWindow);
});

add_task(async function tab_manager_visibility_preference_off() {
  Services.prefs.setBoolPref("browser.tabs.tabmanager.enabled", false);

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: newWindow.gBrowser,
      url: TEST_HOSTNAME + DUMMY_PAGE_PATH,
    },
    async function (browser) {
      await Assert.ok(
        BrowserTestUtils.is_hidden(
          newWindow.document.getElementById("alltabs-button")
        ),
        "tab manage menu is hidden when browser.tabs.tabmanager.enabled preference is set to true"
      );
    }
  );
  Services.prefs.clearUserPref("browser.tabs.tabmanager.enabled");
  BrowserTestUtils.closeWindow(newWindow);
});
