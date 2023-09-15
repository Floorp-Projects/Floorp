const BEHAVIOR_ACCEPT = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

let { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const TOP_DOMAIN = "http://mochi.test:8888/";
const SW_DOMAIN = "https://tracking.example.org/";

const TOP_TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  TOP_DOMAIN
);
const SW_TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  SW_DOMAIN
);

const TOP_EMPTY_PAGE = `${TOP_TEST_ROOT}empty_with_utils.html`;
const SW_REGISTER_PAGE = `${SW_TEST_ROOT}empty_with_utils.html`;
const SW_IFRAME_PAGE = `${SW_TEST_ROOT}page_post_controlled.html`;
// An empty script suffices for our SW needs; it's by definition no-fetch.
const SW_REL_SW_SCRIPT = "empty.js";

/**
 * Set up a no-fetch-optimized ServiceWorker on a domain that will be covered by
 * tracking protection (but is not yet).  Once the SW is installed, activate TP
 * and create a tab that embeds that tracking-site in an iframe.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.testing.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_ACCEPT],
    ],
  });

  // Open the top-level page.
  info("Opening a new tab: " + SW_REGISTER_PAGE);
  let topTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_PAGE,
  });

  // ## Install SW
  info("Installing SW");
  await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ sw: SW_REL_SW_SCRIPT }],
    async function ({ sw }) {
      // Waive the xray to use the content utils.js script functions.
      await content.wrappedJSObject.registerAndWaitForActive(sw);
    }
  );

  // Enable Anti-tracking.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_TRACKER],
    ],
  });
  await UrlClassifierTestUtils.addTestTrackers();

  // Open the top-level URL.
  info("Loading a new top-level URL: " + TOP_EMPTY_PAGE);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    topTab.linkedBrowser
  );
  BrowserTestUtils.startLoadingURIString(topTab.linkedBrowser, TOP_EMPTY_PAGE);
  await browserLoadedPromise;

  // Create Iframe in the top-level page and verify its state.
  let { controlled } = await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ url: SW_IFRAME_PAGE }],
    async function ({ url }) {
      const payload =
        await content.wrappedJSObject.createIframeAndWaitForMessage(url);
      return payload;
    }
  );

  ok(!controlled, "Should not be controlled!");

  // ## Cleanup
  info("Loading the SW unregister page: " + SW_REGISTER_PAGE);
  browserLoadedPromise = BrowserTestUtils.browserLoaded(topTab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(
    topTab.linkedBrowser,
    SW_REGISTER_PAGE
  );
  await browserLoadedPromise;

  await SpecialPowers.spawn(topTab.linkedBrowser, [], async function () {
    await content.wrappedJSObject.unregisterAll();
  });

  // Close the testing tab.
  BrowserTestUtils.removeTab(topTab);
});
