const PATH =
  "http://example.com/browser/dom/tests/mochitest/ajax/offline/";
const URL = PATH + "file_simpleManifest.html";
const MANIFEST = PATH + "file_simpleManifest.cacheManifest";
const PREF_INSECURE_APPCACHE = "browser.cache.offline.insecure.enable";
const PREF_NETWORK_PROXY = "network.proxy.type";

function setSJSState(sjsPath, stateQuery) {
  let client = new XMLHttpRequest();
  client.open("GET", sjsPath + "?state=" + stateQuery, false);
  let appcachechannel = SpecialPowers.wrap(client).channel.QueryInterface(Ci.nsIApplicationCacheChannel);

  return new Promise((resolve, reject) => {
    client.addEventListener("load", resolve);
    client.addEventListener("error", reject);

    appcachechannel.chooseApplicationCache = false;
    appcachechannel.inheritApplicationCache = false;
    appcachechannel.applicationCache = null;

    client.send();
  });
}

add_task(async function() {
  /* This test loads "evil" content and verified it isn't loaded when appcache is disabled, which emulates loading stale cache from an untrusted network:
     - Sets frame to load "evil" content
     - Loads HTML file which also loads and caches content into AppCache
     - Sets frame to load "good"
     - Check we still have "evil" content from AppCache
     - Disables appcache
     - Check content is "good"
  */
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_INSECURE_APPCACHE, true]
    ]
  });
  await setSJSState(PATH + "file_testFile.sjs", "evil");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    let windowPromise = new Promise((resolve, reject) => {
      function init() {
        if (content.document.readyState == "complete") {
          const frame = content.document.getElementById("childframe");
          const state = frame.contentDocument.getElementById("state");
          is(state.textContent, "evil", "Loaded evil content");
          resolve();
        }
      }
      content.document.onreadystatechange = init;
      init();
    });
    let appcachePromise = new Promise((resolve, reject) => {
      function appcacheInit() {
        if (content.applicationCache.status === content.applicationCache.IDLE) {
          ok(true, "Application cache loaded");
          resolve();
        } else {
          info("State was: " + content.applicationCache.status);
        }
      }
      content.applicationCache.oncached = appcacheInit;
      content.applicationCache.onnoupdate = appcacheInit;
      content.applicationCache.onerror = () => {
        ok(false, "Application cache failed");
        reject();
      };
      appcacheInit();
    });
    await Promise.all([windowPromise, appcachePromise]);
  });
  gBrowser.removeCurrentTab();

  // Turn network and proxy off so we can check the content loads still
  await setSJSState(PATH + "file_testFile.sjs", "good");
  Services.cache2.clear();

  // Check we still have the "evil" content despite the state change
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    const frame = content.document.getElementById("childframe");
    const state = frame.contentDocument.getElementById("state");
    is(state.textContent, "evil", "Loaded evil content from cache");
  });
  gBrowser.removeCurrentTab();


  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_INSECURE_APPCACHE, false]
    ]
  });
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  // Check that the "good" content is back now appcache is disabled
  await ContentTask.spawn(gBrowser.selectedBrowser, [URL], async (URL) => {
    const frame = content.document.getElementById("childframe");
    const state = frame.contentDocument.getElementById("state");
    is(state.textContent, "good", "Loaded good content");
    // Eval is needed to execure in child context.
    content.window.eval("OfflineTest.clear()");
  });

  gBrowser.removeCurrentTab();
  await setSJSState(PATH + "file_testFile.sjs", "");
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_pref_removes_api() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_INSECURE_APPCACHE, true]
    ]
  });
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async () => {
    // Have to use in page checking as IsSecureContextOrObjectIsFromSecureContext is true for spawn()
    is(content.document.getElementById("hasAppcache").textContent, "yes", "Appcache is enabled");
    is(content.document.getElementById("hasOfflineResourceList").textContent, "yes", "OfflineResourceList is enabled");
  });
  gBrowser.removeCurrentTab();

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_INSECURE_APPCACHE, false]
    ]
  });
  await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await ContentTask.spawn(gBrowser.selectedBrowser, [URL], async (URL) => {
    is(content.document.getElementById("hasAppcache").textContent, "no", "Appcache is disabled");
    is(content.document.getElementById("hasOfflineResourceList").textContent, "no", "OfflineResourceList is disabled");
    content.window.eval("OfflineTest.clear()");
  });
  gBrowser.removeCurrentTab();
});
