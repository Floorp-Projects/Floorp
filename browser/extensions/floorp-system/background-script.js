'use strict';
browser.browserAction.onClicked.addListener(tab => {
	browser.sessions.getRecentlyClosed(sessionInfos => {
				for (let i = 0; i < sessionInfos.length; i++) {
			let sessionInfo = sessionInfos[i];
			if (sessionInfo.tab && sessionInfo.tab.windowId === tab.windowId) {
				if (sessionInfo.tab.sessionId != null) {chrome.sessions.restore(sessionInfo.tab.sessionId);}else{}break;}}})});

const APP_ID = "floorp"
const API_END_POINT = "https://repo.ablaze.one/api/"

const Notify = (url, now, latest) =>{
    const msg = browser.i18n;
    browser.notifications.create({
        "type": "basic",
        "iconUrl": browser.runtime.getURL("icons/link-48.png"),
        "title": msg.getMessage("notificationTitle"),
        "message": msg.getMessage("notificationContent", [now, latest])
    });
    browser.notifications.onClicked.addListener(() =>{
        browser.tabs.create({
            "url": url
        });
    });
    return null;
};

const NotifyNew = (url, now, latest) =>{
    const msg = browser.i18n;
    browser.notifications.create({
        "type": "basic",
        "iconUrl": browser.runtime.getURL("icons/link-48-last.png"),
        "title": msg.getMessage("notificationTitle"),
        "message": msg.getMessage("notificationContent-last", [now, latest])
    });
    return null;
};

window.onload = () =>{
    (async() => {
        var pref = await browser.aboutConfigPrefs.getPref("floorp.verison")
        var i = await browser.aboutConfigPrefs.getPref("enable.floorp.updater")
        console.log("enable.floorp.updater =" + i)
        console.log("floorp.verison =" + pref)

            fetch(`${API_END_POINT}?name=${APP_ID}`)
            .then(res =>{
                if(res.ok){
                    return res.json()
                }
            }).then(data =>{
                if(data.build != pref){
                    Notify(data.file, pref, data.build);
                    console.log("notificationTitle");
                }
                else if(data.build = pref){
                    NotifyNew();
                    console.log("notificationTitle-last");
                }
            }).then(e =>{
                return e;
             });
        })();
    };

    function initializePageAction(tab) {
        if (typeof tab.url != 'undefined' && tab.url.length ) {
            browser.pageAction.show(tab.id);
        }
    }
    var gettingAllTabs = browser.tabs.query({});
    gettingAllTabs.then((tabs) => {
        for (let tab of tabs) {
            initializePageAction(tab);
        }
    });