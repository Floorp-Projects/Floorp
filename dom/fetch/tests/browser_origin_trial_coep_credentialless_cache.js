const TOP_LEVEL_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "open_credentialless_document.sjs";

const SAME_ORIGIN = "https://example.com";
const CROSS_ORIGIN = "https://test1.example.com";

const RESOURCE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://test1.example.com"
  ) + "credentialless_resource.sjs";

async function store(storer, url, requestCredentialMode) {
  await SpecialPowers.spawn(
    storer.linkedBrowser,
    [url, requestCredentialMode],
    async function(url, requestCredentialMode) {
      const cache = await content.caches.open("v1");
      const fetchRequest = new content.Request(url, {
        mode: "no-cors",
        credentials: requestCredentialMode,
      });

      const fetchResponse = await content.fetch(fetchRequest);
      content.wrappedJSObject.console.log(fetchResponse.headers);
      await cache.put(fetchRequest, fetchResponse);
    }
  );
}

async function retrieve(retriever, resourceURL) {
  return await SpecialPowers.spawn(
    retriever.linkedBrowser,
    [resourceURL],
    async function(url) {
      const cache = await content.caches.open("v1");
      try {
        await cache.match(url);
        return "retrieved";
      } catch (error) {
        return "error";
      }
    }
  );
}

async function testCache(
  storer,
  storeRequestCredentialMode,
  resourceCOEP,
  retriever,
  expectation
) {
  const resourceURL = RESOURCE_URL + "?" + resourceCOEP;

  await store(storer, resourceURL, storeRequestCredentialMode);
  const result = await retrieve(retriever, resourceURL);

  is(result, expectation);
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.coep.credentialless", false],
      ["dom.origin-trials.enabled", true],
      ["dom.origin-trials.test-key.enabled", true],
    ],
  });

  const noneTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TOP_LEVEL_URL
  );
  const requireCorpTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TOP_LEVEL_URL + "?requirecorp"
  );
  const credentiallessTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TOP_LEVEL_URL + "?credentialless"
  );

  await testCache(noneTab, "include", "", noneTab, "retrieved");
  await testCache(noneTab, "include", "", credentiallessTab, "error");
  await testCache(noneTab, "omit", "", credentiallessTab, "retrieved");
  await testCache(
    noneTab,
    "include",
    "corp_cross_origin",
    credentiallessTab,
    "retrieved"
  );
  await testCache(noneTab, "include", "", requireCorpTab, "error");
  await testCache(
    noneTab,
    "include",
    "corp_cross_origin",
    requireCorpTab,
    "retrieved"
  );
  await testCache(credentiallessTab, "include", "", noneTab, "retrieved");
  await testCache(
    credentiallessTab,
    "include",
    "",
    credentiallessTab,
    "retrieved"
  );
  await testCache(credentiallessTab, "include", "", requireCorpTab, "error");
  await testCache(
    requireCorpTab,
    "include",
    "corp_cross_origin",
    noneTab,
    "retrieved"
  );
  await testCache(
    requireCorpTab,
    "include",
    "corp_cross_origin",
    credentiallessTab,
    "retrieved"
  );
  await testCache(
    requireCorpTab,
    "include",
    "corp_cross_origin",
    requireCorpTab,
    "retrieved"
  );

  await BrowserTestUtils.removeTab(noneTab);
  await BrowserTestUtils.removeTab(requireCorpTab);
  await BrowserTestUtils.removeTab(credentiallessTab);
});
