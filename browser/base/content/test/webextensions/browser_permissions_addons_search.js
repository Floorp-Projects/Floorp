"use strict";

async function installSearch(filename) {
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.getAddons.maxResults", 10],
    ["extensions.getAddons.search.url", `${BASE}/browser_webext_search.xml`],
  ]});

  let win = await BrowserOpenAddonsMgr("addons://list/extension");

  let searchResultsPromise = new Promise(resolve => {
    win.document.addEventListener("ViewChanged", resolve, {once: true});
  });
  let search = win.document.getElementById("header-search");
  search.focus();
  search.value = "search text";
  EventUtils.synthesizeKey("VK_RETURN", {}, win);

  await searchResultsPromise;
  ok(win.gViewController.currentViewId.startsWith("addons://search"),
     "about:addons is displaying search results");

  let list = win.document.getElementById("search-list");
  let item = null;
  for (let child of list.childNodes) {
    if (child.nodeName == "richlistitem" &&
        child.mAddon.install.sourceURI.pathQueryRef.endsWith(filename)) {
          item = child;
          break;
    }
  }
  ok(item, `Found ${filename} in search results`);

  // abracadabara XBL
  item.clientTop;

  let install = win.document.getAnonymousElementByAttribute(item, "anonid", "install-status");
  let button = win.document.getAnonymousElementByAttribute(install, "anonid", "install-remote-btn");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
}

add_task(() => testInstallMethod(installSearch, "installAmo"));
