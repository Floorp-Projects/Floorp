const TOP_LEVEL_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "open_credentialless_document.sjs";

const WORKER_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "credentialless_worker.sjs";

const GET_STATE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "store_header.sjs?getstate";

const SAME_ORIGIN = "https://example.com";
const CROSS_ORIGIN = "https://test1.example.com";

const WORKER_USES_CREDENTIALLESS = "credentialless";
const WORKER_NOT_USE_CREDENTIALLESS = "";

async function addCookieToOrigin(origin) {
  const fetchRequestURL =
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    "store_header.sjs?addcookie";

  const addcookieTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    fetchRequestURL
  );

  await SpecialPowers.spawn(addcookieTab.linkedBrowser, [], async function() {
    content.document.cookie = "coep=credentialless; SameSite=None; Secure";
  });
  await BrowserTestUtils.removeTab(addcookieTab);
}

async function testOrigin(
  fetchOrigin,
  isCredentialless,
  workerUsesCredentialless,
  expectedCookieResult
) {
  let topLevelUrl = TOP_LEVEL_URL;
  if (isCredentialless) {
    topLevelUrl += "?credentialless";
  }
  const noCredentiallessTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    topLevelUrl
  );

  const fetchRequestURL =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      fetchOrigin
    ) + "store_header.sjs?checkheader";

  let workerScriptURL = WORKER_URL + "?" + workerUsesCredentialless;

  await SpecialPowers.spawn(
    noCredentiallessTab.linkedBrowser,
    [fetchRequestURL, GET_STATE_URL, workerScriptURL, expectedCookieResult],
    async function(
      fetchRequestURL,
      getStateURL,
      workerScriptURL,
      expectedCookieResult
    ) {
      const worker = new content.Worker(workerScriptURL, {});

      // When the worker receives this message, it'll send
      // a fetch request to fetchRequestURL, and fetchRequestURL
      // will store whether it has received the cookie as a
      // shared state.
      worker.postMessage(fetchRequestURL);

      if (expectedCookieResult == "error") {
        await new Promise(r => {
          worker.onerror = function() {
            ok(true, "worker has error");
            r();
          };
        });
      } else {
        await new Promise(r => {
          worker.addEventListener("message", async function() {
            // This request is used to get the saved state from the
            // previous fetch request.
            const response = await content.fetch(getStateURL, {
              mode: "cors",
            });
            const text = await response.text();
            is(text, expectedCookieResult);
            r();
          });
        });
      }
    }
  );
  await BrowserTestUtils.removeTab(noCredentiallessTab);
}

async function dedicatedWorkerTest(
  origin,
  workerCOEP,
  expectedCookieResultForNoCredentialless,
  expectedCookieResultForCredentialless
) {
  await testOrigin(
    origin,
    false,
    workerCOEP,
    expectedCookieResultForNoCredentialless
  );
  await testOrigin(
    origin,
    true,
    workerCOEP,
    expectedCookieResultForCredentialless
  );
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.coep.credentialless", false], // Explicitly set credentialless to false because we want to test origin trial
      ["dom.origin-trials.enabled", true],
      ["dom.origin-trials.test-key.enabled", true],
    ],
  });

  await addCookieToOrigin(SAME_ORIGIN);
  await addCookieToOrigin(CROSS_ORIGIN);

  await dedicatedWorkerTest(
    SAME_ORIGIN,
    WORKER_NOT_USE_CREDENTIALLESS,
    "hasCookie",
    "error"
  );
  await dedicatedWorkerTest(
    SAME_ORIGIN,
    WORKER_USES_CREDENTIALLESS,
    "hasCookie",
    "hasCookie"
  );

  await dedicatedWorkerTest(
    CROSS_ORIGIN,
    WORKER_NOT_USE_CREDENTIALLESS,
    "hasCookie",
    "error"
  );
  await dedicatedWorkerTest(
    CROSS_ORIGIN,
    WORKER_USES_CREDENTIALLESS,
    "noCookie",
    "noCookie"
  );
});
