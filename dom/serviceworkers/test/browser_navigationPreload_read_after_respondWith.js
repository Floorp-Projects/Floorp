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
const SW_IFRAME_PAGE = `${SW_TEST_ROOT}navigationPreload_page.html`;
// An empty script suffices for our SW needs; it's by definition no-fetch.
const SW_REL_SW_SCRIPT = "sw_with_navigationPreload.js";

/**
 * Test the FetchEvent.preloadResponse can be read after FetchEvent.respondWith()
 *
 * Step 1. register a ServiceWorker which only handles FetchEvent when request
 *         url includes navigationPreload_page.html. Otherwise, it alwasy
 *         fallbacks the fetch to the network.
 *         If the request url includes navigationPreload_page.html, it call
 *         FetchEvent.respondWith() with a new Resposne, and then call
 *         FetchEvent.waitUtil() to wait FetchEvent.preloadResponse and post the
 *         preloadResponse's text to clients.
 * Step 2. Open a controlled page and register message event handler to receive
 *         the postMessage from ServiceWorker.
 * Step 3. Create a iframe which url is navigationPreload_page.html, such that
 *         ServiceWorker can fake the response and then send preloadResponse's
 *         result.
 * Step 4. Unregister the ServiceWorker and cleanup the environment.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });

  // Step 1.
  info("Opening a new tab: " + SW_REGISTER_PAGE);
  let topTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_PAGE,
  });

  // ## Install SW
  await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ sw: SW_REL_SW_SCRIPT }],
    async function ({ sw }) {
      // Waive the xray to use the content utils.js script functions.
      dump(`register serviceworker...\n`);
      await content.wrappedJSObject.registerAndWaitForActive(sw);
    }
  );

  // Step 2.
  info("Loading a controlled page: " + SW_REGISTER_PAGE);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    topTab.linkedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    topTab.linkedBrowser,
    SW_REGISTER_PAGE
  );
  await browserLoadedPromise;

  info("Create a target iframe: " + SW_IFRAME_PAGE);
  let result = await SpecialPowers.spawn(
    topTab.linkedBrowser,
    [{ url: SW_IFRAME_PAGE }],
    async function ({ url }) {
      async function waitForNavigationPreload() {
        return new Promise(resolve => {
          content.wrappedJSObject.navigator.serviceWorker.addEventListener(
            `message`,
            event => {
              resolve(event.data);
            }
          );
        });
      }

      let promise = waitForNavigationPreload();

      // Step 3.
      const iframe = content.wrappedJSObject.document.createElement("iframe");
      iframe.src = url;
      content.wrappedJSObject.document.body.appendChild(iframe);
      await new Promise(r => {
        iframe.onload = r;
      });

      let result = await promise;
      return result;
    }
  );

  is(result, "NavigationPreload\n", "Should get NavigationPreload result");

  // Step 4.
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
