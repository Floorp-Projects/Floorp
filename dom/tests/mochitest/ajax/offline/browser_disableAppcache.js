const PATH =
  "http://example.com/browser/dom/tests/mochitest/ajax/offline/";
const URL = PATH + "file_simpleManifest.html";
const MANIFEST = PATH + "file_simpleManifest.cacheManifest";
const PREF_NETWORK_PROXY = "network.proxy.type";

add_task(async function test_pref_removes_api() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    // Have to use in page checking as IsSecureContextOrObjectIsFromSecureContext is true for spawn()
    is(content.document.getElementById("hasAppcache").textContent, "no", "Appcache is disabled");
    is(content.document.getElementById("hasOfflineResourceList").textContent, "no", "OfflineResourceList is disabled");
  });
  gBrowser.removeCurrentTab();
});
