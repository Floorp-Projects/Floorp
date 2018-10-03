
const BEHAVIOR_ACCEPT         = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

let {UrlClassifierTestUtils} =
  ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const TOP_DOMAIN = "http://mochi.test:8888/";
const SW_DOMAIN = "https://tracking.example.org/";

const TOP_TEST_ROOT = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", TOP_DOMAIN);
const SW_TEST_ROOT = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content/", SW_DOMAIN);

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
add_task(async function() {
  await SpecialPowers.pushPrefEnv({'set': [
    ['dom.serviceWorkers.enabled', true],
    ['dom.serviceWorkers.exemptFromPerDomainMax', true],
    ['dom.serviceWorkers.testing.enabled', true],
    ['network.cookie.cookieBehavior', BEHAVIOR_ACCEPT],
  ]});

  // ## Install SW
  info("Installing SW");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SW_REGISTER_PAGE
    },
    async function(linkedBrowser) {
      await ContentTask.spawn(
        linkedBrowser,
        { sw: SW_REL_SW_SCRIPT },
        async function({ sw }) {
          // Waive the xray to use the content utils.js script functions.
          await content.wrappedJSObject.registerAndWaitForActive(sw);
        }
      );
    }
  );

  // Enable Anti-tracking.
  await SpecialPowers.pushPrefEnv({'set': [
    ['privacy.trackingprotection.enabled', false],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["privacy.trackingprotection.annotate_channels", true],
    ['network.cookie.cookieBehavior', BEHAVIOR_REJECT_TRACKER],
  ]});
  await UrlClassifierTestUtils.addTestTrackers();

  // Open the top-level page.
  info("Open top-level page");
  let topTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TOP_EMPTY_PAGE
  });

  // Create Iframe in the top-level page and verify its state.
  let { controlled } = await ContentTask.spawn(
    topTab.linkedBrowser,
    { url: SW_IFRAME_PAGE },
    async function ({ url }) {
      const payload =
        await content.wrappedJSObject.createIframeAndWaitForMessage(url);
      return payload;
    }
  );

  // Currently TP will allow this, although it probably shouldn't?
  ok(controlled, "Should be controlled (and not crash.)");

  // ## Cleanup
  // Close the testing tab.
  BrowserTestUtils.removeTab(topTab);
  // Unregister the SW we registered for the tracking protection origin.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SW_REGISTER_PAGE
    },
    async function(linkedBrowser) {
      await ContentTask.spawn(
        linkedBrowser,
        {},
        async function() {
          // Waive the xray to use the content utils.js script functions.
          await content.wrappedJSObject.unregisterAll();
        }
      );
    }
  );
});
