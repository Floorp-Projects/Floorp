/**
 * Bug 1339336 - A test case for testing pref 'privacy.firstparty.isolate.restrict_opener_access'
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const FIRST_PARTY_OPENER = "example.com";
const FIRST_PARTY_TARGET = "example.org";
const OPENER_PAGE = "http://" + FIRST_PARTY_OPENER + "/browser/browser/components/" +
                    "originattributes/test/browser/file_windowOpenerRestriction.html";
const TARGET_PAGE = "http://" + FIRST_PARTY_TARGET + "/browser/browser/components/" +
                    "originattributes/test/browser/file_windowOpenerRestrictionTarget.html";

async function testPref(aIsPrefEnabled) {
  // Use a random key so we don't access it in later tests.
  let cookieStr = "key" + Math.random().toString() + "=" + Math.random().toString();

  // Open the tab for the opener page.
  let tab = BrowserTestUtils.addTab(gBrowser, OPENER_PAGE);

  // Select this tab and make sure its browser is loaded and focused.
  gBrowser.selectedTab = tab;
  tab.ownerGlobal.focus();

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, {cookieStr,
                                    page: TARGET_PAGE,
                                    isPrefEnabled: aIsPrefEnabled}, async function(obj) {
    // Acquire the iframe element.
    let childFrame = content.document.getElementById("child");

    // Insert a cookie into this iframe.
    childFrame.contentDocument.cookie = obj.cookieStr;

    // Open the tab here and focus on it.
    let openedPath = obj.page;
    if (!obj.isPrefEnabled) {
      // If the pref is not enabled, we pass the cookie value through the query string
      // to tell the target page that it should check the cookie value.
      openedPath += "?" + obj.cookieStr;
    }

    // Issue the opener page to open the target page and focus on it.
    this.openedWindow = content.open(openedPath);
    this.openedWindow.focus();
  });

  // Wait until the target page is loaded.
  let targetBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  await BrowserTestUtils.browserLoaded(targetBrowser);

  // The target page will do the check and show the result through its title.
  is(targetBrowser.contentTitle, "pass", "The behavior of window.opener is correct.");

  // Close Tabs.
  await ContentTask.spawn(browser, null, async function() {
    this.openedWindow.close();
  });
  await BrowserTestUtils.removeTab(tab);

  // Reset cookies
  Services.cookies.removeAll();
}

add_task(async function runTests() {
  let tests = [true, false];

  // First, we test the scenario that the first party isolation is enabled.
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.firstparty.isolate", true]]
  });

  for (let enabled of tests) {
    await SpecialPowers.pushPrefEnv({"set":
      [["privacy.firstparty.isolate.restrict_opener_access", enabled]]
    });

    await testPref(enabled);
  }

  // Second, we test the scenario that the first party isolation is disabled.
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.firstparty.isolate", false]]
  });

  for (let enabled of tests) {
    await SpecialPowers.pushPrefEnv({"set":
      [["privacy.firstparty.isolate.restrict_opener_access", enabled]]
    });

    // When first party isolation is disabled, this pref will not affect the behavior of
    // window.opener. And the correct behavior here is to allow access since the iframe in
    // the opener page has the same origin with the target page.
    await testPref(false);
  }
});
