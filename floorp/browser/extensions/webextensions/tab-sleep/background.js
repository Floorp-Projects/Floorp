const TAB_SLEEP_ENABLED_PREF = "floorp.tabsleep.enabled";
const TAB_SLEEP_TESTMODE_ENABLED_PREF = "floorp.tabsleep.testmode.enabled";
const TAB_SLEEP_TAB_TIMEOUT_SECONDS_PREF = "floorp.tabsleep.tabTimeoutSeconds";

const EXCLUDE_URL_PATTERNS = [
    "auth",
    "verify",
    "verification",
    "create",
    "delete",
    "remove",
    "move",
    "edit",
    "signin",
    "signup",
    "sign-in",
    "sign-up",
    "login",
    "log-in",
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


(async function main() {
    let isEnabled = await browser.aboutConfigPrefs.getPref(TAB_SLEEP_ENABLED_PREF);
    browser.aboutConfigPrefs.onPrefChange.addListener(function() {
        browser.runtime.reload();
    }, TAB_SLEEP_ENABLED_PREF);

    let isTestMode = await browser.aboutConfigPrefs.getPref(TAB_SLEEP_TESTMODE_ENABLED_PREF);
    if (isTestMode) console.log("Test mode is enabled");

    let systemMemory = await browser.memoryInfo.getSystemMemorySize();
    let systemMemoryGB = systemMemory / 1024 / 1024 / 1024;
    console.log(`System Memory (GB): ${systemMemoryGB}`);

    let TAB_TIMEOUT_SECONDS_DEFAULT = Math.floor(60 * (systemMemoryGB * 5));
    await browser.aboutConfigPrefs.setDefaultIntPref(TAB_SLEEP_TAB_TIMEOUT_SECONDS_PREF, TAB_TIMEOUT_SECONDS_DEFAULT);
    let TAB_TIMEOUT_SECONDS;
    let tabTimeoutMiliseconds;
    async function prefHandle() {
        TAB_TIMEOUT_SECONDS = await browser.aboutConfigPrefs.getIntPref(TAB_SLEEP_TAB_TIMEOUT_SECONDS_PREF);
        tabTimeoutMiliseconds = TAB_TIMEOUT_SECONDS * 1000;
        console.log(`TAB_TIMEOUT_SECONDS: ${TAB_TIMEOUT_SECONDS}`);
    }
    await prefHandle();
    browser.aboutConfigPrefs.onPrefChange.addListener(prefHandle, TAB_SLEEP_TAB_TIMEOUT_SECONDS_PREF);

    browser.runtime.onMessage.addListener(function(request, sender, sendResponse) {
        if (request["request"] === "status-data") {
            sendResponse({
                response: "status-data",
                data: {
                    status: {
                        systemMemory: systemMemory,
                        tabSleepEnabled: Boolean(isEnabled),
                        testModeEnabled: Boolean(isTestMode),
                        tabTimeoutSeconds: TAB_TIMEOUT_SECONDS,
                    }
                }
            });
        }
    });

    if (!isEnabled) return;

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

    browser.runtime.onMessage.addListener(function(request, sender, sendResponse) {
        (async() => {
            let tabsLastActivity_data = [];
            let keys = Object.keys(tabsLastActivity);
            for (let key of keys) {
                tabsLastActivity_data.push({
                    title: (await browser.tabs.get(Number(key))).title,
                    lastActivity: tabsLastActivity[key],
                })
            }
            if (request["request"] === "tabs-last-activity-data") {
                sendResponse({
                    response: "tabs-last-activity-data",
                    data: {
                        tabsLastActivity: tabsLastActivity_data
                    }
                });
            }
        })();
        return true;
    });

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
