var gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

add_task(async function () {
  registerCleanupFunction(function () {
    gBrowser.removeCurrentTab();
    window.focus();
  });
});

// simple tab load helper, pilfered from browser plugin tests
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  }

  return loaded;
}

add_task(async function () {
  let pluginTab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  await promiseTabLoadEvent(pluginTab, gTestRoot + "block_all_plugins.html");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let doc = content.document;

    let objectElt = doc.getElementById("object");
    Assert.ok(!!objectElt, "object should exist");
    Assert.ok(
      objectElt instanceof Ci.nsIObjectLoadingContent,
      "object should be an nsIObjectLoadingContent"
    );
    Assert.ok(
      objectElt.displayedType == Ci.nsIObjectLoadingContent.TYPE_FALLBACK,
      "object should be blocked"
    );

    let embedElt = doc.getElementById("embed");
    Assert.ok(!!embedElt, "embed should exist");
    Assert.ok(
      embedElt instanceof Ci.nsIObjectLoadingContent,
      "embed should be an nsIObjectLoadingContent"
    );
    Assert.ok(
      embedElt.displayedType == Ci.nsIObjectLoadingContent.TYPE_FALLBACK,
      "embed should be blocked"
    );
  });
});
