"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const { Management } = ChromeUtils.importESModule(
  "resource://gre/modules/Extension.sys.mjs"
);

const PREF_WC_REPORTER_ENABLED = "extensions.webcompat-reporter.enabled";
const PREF_WC_REPORTER_ENDPOINT =
  "extensions.webcompat-reporter.newIssueEndpoint";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_PAGE = TEST_ROOT + "test.html";
const FRAMEWORKS_TEST_PAGE = TEST_ROOT + "frameworks.html";
const FASTCLICK_TEST_PAGE = TEST_ROOT + "fastclick.html";
const NEW_ISSUE_PAGE = TEST_ROOT + "webcompat.html";

const WC_ADDON_ID = "webcompat-reporter@mozilla.org";

async function promiseAddonEnabled() {
  const addon = await AddonManager.getAddonByID(WC_ADDON_ID);
  if (addon.isActive) {
    return;
  }
  const pref = SpecialPowers.Services.prefs.getBoolPref(
    PREF_WC_REPORTER_ENABLED,
    false
  );
  if (!pref) {
    SpecialPowers.Services.prefs.setBoolPref(PREF_WC_REPORTER_ENABLED, true);
  }
}

class HelpMenuHelper {
  #popup = null;

  async open() {
    this.popup = document.getElementById("menu_HelpPopup");
    ok(this.popup, "Help menu should exist");

    const menuOpen = BrowserTestUtils.waitForEvent(this.popup, "popupshown");

    // This event-faking method was copied from browser_title_case_menus.js so
    // this can be tested on MacOS (where the actual menus cannot be opened in
    // tests, but we only need the help menu to populate itself).
    this.popup.dispatchEvent(new MouseEvent("popupshowing", { bubbles: true }));
    this.popup.dispatchEvent(new MouseEvent("popupshown", { bubbles: true }));

    await menuOpen;
  }

  async close() {
    if (this.popup) {
      const menuClose = BrowserTestUtils.waitForEvent(
        this.popup,
        "popuphidden"
      );

      // (Also copied from browser_title_case_menus.js)
      // Just for good measure, we'll fire the popuphiding/popuphidden events
      // after we close the menupopups.
      this.popup.dispatchEvent(
        new MouseEvent("popuphiding", { bubbles: true })
      );
      this.popup.dispatchEvent(
        new MouseEvent("popuphidden", { bubbles: true })
      );

      await menuClose;
      this.popup = null;
    }
  }

  isItemHidden() {
    const item = document.getElementById("help_reportSiteIssue");
    return item && item.hidden;
  }

  isItemEnabled() {
    const item = document.getElementById("help_reportSiteIssue");
    return item && !item.hidden && !item.disabled;
  }
}

async function startIssueServer() {
  const landingTemplate = await new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", NEW_ISSUE_PAGE);
    xhr.onload = () => {
      resolve(xhr.responseText);
    };
    xhr.onerror = reject;
    xhr.send();
  });

  const { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );
  const server = new HttpServer();

  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => server.stop(resolve));
  });

  server.registerPathHandler("/new", function (request, response) {
    response.setHeader("Content-Type", "text/html", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(landingTemplate);
  });

  server.start(-1);
  return `http://localhost:${server.identity.primaryPort}/new`;
}
