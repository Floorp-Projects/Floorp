const EXCLUDE_URL_PATTEANS = [
  "create",
  "delete",
  "edit",
  "signin",
  "signup",
  "login",
  "wp-admin",
  "cart",
  "watch",
  "upload",
  "^about:",
  "^chrome:\/\/",
  "^resource:\/\/",
];

const BROWSER_CACHE_MEMORY_ENABLE_PREF = "browser.cache.memory.enable";
const BROWSER_CACHE_DISK_ENABLE_PREF = "browser.cache.disk.enable";

const tabsLastActivity = {};

function handleUpdateActivity(tabId) {
  if (typeof tabId !== "number") return;
  tabsLastActivity[String(tabId)] = (new Date()).getTime();
}
function handleRemoveActivity(tabId) {
  if (typeof tabId !== "number") return;
  delete tabsLastActivity[String(tabId)];
}
browser.tabs.onActivated.addListener(function(activeInfo) {
  handleUpdateActivity(activeInfo.tabId);
});
//browser.tabs.onAttached.addListener(handleUpdateActivity);
browser.tabs.onCreated.addListener(function(tab) {
  handleUpdateActivity(tab.id);
});
//browser.tabs.onDetached.addListener(handleUpdateActivity);
/*
browser.tabs.onHighlighted.addListener(function(highlightInfo) {
  for (let tabId of highlightInfo.tabIds) {
    handleUpdateActivity(tabId);
  }
});
*/
//browser.tabs.onMoved.addListener(handleUpdateActivity);
browser.tabs.onRemoved.addListener(handleRemoveActivity);
browser.tabs.onReplaced.addListener(function(addedTabId, removedTabId) {
  handleRemoveActivity(removedTabId);
  handleUpdateActivity(addedTabId);
});
browser.tabs.onUpdated.addListener(function(tabId, changeInfo) {
  if (
    typeof changeInfo.attention !== "undefined" ||
    typeof changeInfo.audible !== "undefined" ||
    typeof changeInfo.isArticle !== "undefined" ||
    typeof changeInfo.mutedInfo !== "undefined" ||
    typeof changeInfo.pinned !== "undefined" ||
    typeof changeInfo.status !== "undefined" ||
    typeof changeInfo.url !== "undefined"
  ) {
    handleUpdateActivity(tabId);
  }
});

(async () => {
  let isTestMode = await browser.aboutConfigPrefs.getBoolPref("floorp.tabsleep.testmode.enabled");

  let sysMemGB = await browser.memoryInfo.getSystemMemorySize() / 1024 / 1024 / 1024;
  if (sysMemGB <= 3) {
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_MEMORY_ENABLE_PREF, false);
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_DISK_ENABLE_PREF, true);
  } else if (sysMemGB >= 30) {
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_MEMORY_ENABLE_PREF, true);
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_DISK_ENABLE_PREF, false);
  } else {
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_MEMORY_ENABLE_PREF, true);
    await browser.aboutConfigPrefs.setBoolPref(BROWSER_CACHE_DISK_ENABLE_PREF, true);
  }

  let TAB_TIMEOUT_MILISEC = 60 * 1000 * 60;
  if (sysMemGB <= 2) {
    TAB_TIMEOUT_MILISEC = 60 * 1000 * 3;
  } else {
    TAB_TIMEOUT_MILISEC = 60 * 1000 * (sysMemGB * 3);
  }
  if (isTestMode) {
    TAB_TIMEOUT_MILISEC = 60 * 1000 * 2;
  }

  /*
  await new Promise(resolve => {
    setTimeout(resolve, 30000);
  });
  */
  /*
  let getBrowserUsageMemoryStart = new Date();
  let memory = await browser.memoryInfo.getBrowserUsageMemorySize();
  let getBrowserUsageMemoryEnd = new Date();

  let enabledGetBrowserUsageMemory = true;
  if ((getBrowserUsageMemoryEnd.getTime() - getBrowserUsageMemoryStart.getTime()) > 800) {
    console.log("Disable getBrowserUsageMemory");
    enabledGetBrowserUsageMemory = false;
  }
  */

  //Check tab lastAccessed
  setInterval(async function () {
    let tabs = await browser.tabs.query({
      active: false,
      attention: false,
      audible: false,
      discarded: false,
      highlighted: false,
      pinned: false,
      status: "complete"
    });
    for (let tab of tabs) {
      let target = true;
      for (let EXCLUDE_URL_PATTEAN of EXCLUDE_URL_PATTEANS) {
        if ((new RegExp(EXCLUDE_URL_PATTEAN)).test(tab.url)) {
          target = false;
        }
      }
      if (!target) continue;

      if (
        ((new Date()).getTime() - tab.lastAccessed) > TAB_TIMEOUT_MILISEC &&
        (
          typeof tabsLastActivity[tab.id] === "undefined" ||
          ((new Date()).getTime() - tabsLastActivity[tab.id]) > TAB_TIMEOUT_MILISEC
        )
      ) {
        try {
          await browser.tabs.discard(tab.id);
          if (isTestMode) {
            console.log(`${tab.title} (${tab.id}): discarded`);
          }
        } catch (e) { console.error(e) }
      }
    }
  }, 60 * 1000);
})();