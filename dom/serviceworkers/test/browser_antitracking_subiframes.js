const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

const TOP_DOMAIN = "http://mochi.test:8888/";
const SW_DOMAIN = "https://example.org/";

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
const SW_REL_SW_SCRIPT = "empty.js";

/**
 * Set up a ServiceWorker on a domain that will be used as 3rd party iframe.
 * That 3rd party frame should be controlled by the ServiceWorker.
 * After that, we open a second iframe into the first one. That should not be
 * controlled.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.testing.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_TRACKER],
    ],
  });

  // Open the top-level page.
  info("Opening a new tab: " + SW_REGISTER_PAGE);
  let topTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_PAGE,
  });

  // Install SW
  info("Registering a SW: " + SW_REL_SW_SCRIPT);
  await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ sw: SW_REL_SW_SCRIPT }],
    async function ({ sw }) {
      // Waive the xray to use the content utils.js script functions.
      await content.wrappedJSObject.registerAndWaitForActive(sw);
      // User interaction
      content.document.userInteractionForTesting();
    }
  );

  info("Loading a new top-level URL: " + TOP_EMPTY_PAGE);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    topTab.linkedBrowser
  );
  BrowserTestUtils.startLoadingURIString(topTab.linkedBrowser, TOP_EMPTY_PAGE);
  await browserLoadedPromise;

  // Create Iframe in the top-level page and verify its state.
  info("Creating iframe and checking if controlled");
  let { controlled } = await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ url: SW_IFRAME_PAGE }],
    async function ({ url }) {
      content.document.userInteractionForTesting();
      const payload =
        await content.wrappedJSObject.createIframeAndWaitForMessage(url);
      return payload;
    }
  );

  ok(controlled, "Should be controlled!");

  // Create a nested Iframe.
  info("Creating nested-iframe and checking if controlled");
  let { nested_controlled } = await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ url: SW_IFRAME_PAGE }],
    async function ({ url }) {
      const payload =
        await content.wrappedJSObject.createNestedIframeAndWaitForMessage(url);
      return payload;
    }
  );

  ok(!nested_controlled, "Should not be controlled!");

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
