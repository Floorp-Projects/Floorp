const EXCLUDE_URL_PATTERNS = [
    "create",
    "delete",
    "edit",
    "signin",
    "signup",
    "login",
    "modify",
    "form",
    "settings",
    "token",
    "secure",
    "secret",
    "new",
    "wp-admin",
    "cart",
    "upload",
    "^about:",
    "^chrome:\/\/",
    "^resource:\/\/",
];

const EXCLUDE_URL_PATTERNS_COMPILED = [];

for (let EXCLUDE_URL_PATTERN of EXCLUDE_URL_PATTERNS) {
    EXCLUDE_URL_PATTERNS_COMPILED.push(new RegExp(EXCLUDE_URL_PATTERN));
}


// Monitor tab activity.
let tabsLastActivity = {};
function onTabsActivity(tabId) {
    if (typeof tabId !== "number") return;
    tabsLastActivity[String(tabId)] = (new Date()).getTime();
}
function onTabsRemoveActivity(tabId) {
    if (typeof tabId !== "number") return;
    delete tabsLastActivity[String(tabId)];
}
browser.tabs.onActivated.addListener(function (activeInfo) {
    onTabsActivity(activeInfo.tabId);
});
browser.tabs.onCreated.addListener(function (tab) {
    onTabsActivity(tab.id);
});
browser.tabs.onRemoved.addListener(onTabsRemoveActivity);
browser.tabs.onReplaced.addListener(function (addedTabId, removedTabId) {
    onTabsRemoveActivity(removedTabId);
    onTabsActivity(addedTabId);
});
browser.tabs.onUpdated.addListener(function (tabId, changeInfo) {
    if (
        typeof changeInfo.attention !== "undefined" ||
        typeof changeInfo.audible !== "undefined" ||
        typeof changeInfo.isArticle !== "undefined" ||
        typeof changeInfo.mutedInfo !== "undefined" ||
        typeof changeInfo.pinned !== "undefined" ||
        typeof changeInfo.status !== "undefined" ||
        typeof changeInfo.url !== "undefined"
    ) {
        onTabsActivity(tabId);
    }
});


(async function main() {
    let isTestMode = await browser.aboutConfigPrefs.getPref("floorp.tabsleep.testmode.enabled");
    if (isTestMode) console.log("Test mode is enabled");

    let systemMemory = await browser.memoryInfo.getSystemMemorySize();
    let systemMemoryGB = systemMemory / 1024 / 1024 / 1024;
    console.log(`System Memory (GB): ${systemMemoryGB}`);

    let TAB_TIMEOUT_SECONDS;
    let tabTimeoutMiliseconds;
    async function prefHandle() {
        TAB_TIMEOUT_SECONDS = Math.floor(60 * (systemMemoryGB * 3));
        try {
            let pref = await browser.aboutConfigPrefs.getIntPref("floorp.tabsleep.tabTimeoutSeconds");
            if (pref !== 0) TAB_TIMEOUT_SECONDS = pref;
        } catch (e) {
            console.error(e);
        }
        tabTimeoutMiliseconds = TAB_TIMEOUT_SECONDS * 1000;
        console.log(`TAB_TIMEOUT_SECONDS: ${TAB_TIMEOUT_SECONDS}`);
    }
    await prefHandle();
    browser.aboutConfigPrefs.onPrefChange.addListener(prefHandle, "floorp.tabsleep.tabTimeoutSeconds");

    setInterval(async function() {
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
            for (let EXCLUDE_URL_PATTERN_COMPILED of EXCLUDE_URL_PATTERNS_COMPILED) {
                if (EXCLUDE_URL_PATTERN_COMPILED.test(tab.url)) {
                    target = false;
                }
            }
            if (!target) continue;
            if (
                ((new Date()).getTime() - tab.lastAccessed) > tabTimeoutMiliseconds &&
                (
                    typeof tabsLastActivity[tab.id] === "undefined" ||
                    ((new Date()).getTime() - tabsLastActivity[tab.id]) > tabTimeoutMiliseconds
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