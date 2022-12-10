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
browser.tabs.onCreated.addListener(function(tab) {
  handleUpdateActivity(tab.id);
});
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

async function setMemCache(enabled) {
  await browser.aboutConfigPrefs.setBoolPref("browser.cache.memory.enable", enabled);
}
async function setDiskCache(enabled) {
  await browser.aboutConfigPrefs.setBoolPref("browser.cache.disk.enable", enabled);
}

(async () => {
  let isTestMode = await browser.aboutConfigPrefs.getPref("floorp.tabsleep.testmode.enabled");
  if (isTestMode) console.log("Test mode is enabled");

  let sysMemGB = await browser.memoryInfo.getSystemMemorySize() / 1024 / 1024 / 1024;
  if (sysMemGB <= 3) {
    await setMemCache(false);
    await setDiskCache(true);
  } else if (sysMemGB >= 30) {
    await setMemCache(true);
    await setDiskCache(false);
  } else {
    await setMemCache(true);
    await setDiskCache(true);
  }

  let TAB_TIMEOUT_MILISEC = 60 * 1000 * (sysMemGB * 3);
  let tabTimeoutMilisecPref = await browser.aboutConfigPrefs.getPref("floorp.tabsleep.tabTimeoutMiliseconds");
  if (tabTimeoutMilisecPref !== undefined &&
     tabTimeoutMilisecPref !== 0) {
    TAB_TIMEOUT_MILISEC = tabTimeoutMilisecPref;
  }
  if (isTestMode) {
    TAB_TIMEOUT_MILISEC = 60 * 1000 * 2;
  }

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