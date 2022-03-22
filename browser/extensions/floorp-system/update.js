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

const NotifyNew = (now, latest) =>{
    const msg = browser.i18n;
    browser.notifications.create({
        "type": "basic",
        "iconUrl": browser.runtime.getURL("icons/link-48-last.png"),
        "title": msg.getMessage("notificationTitle"),
        "message": msg.getMessage("notificationContent-last", [now, latest])
    });
    return null;
};

const CheckUpdate = async(options) =>{
    var updaterEnabled = await browser.aboutConfigPrefs.getPref("enable.floorp.update");
    if(!updaterEnabled){
        return; //updater is disabled
    }
    var latestNotify;
    if(options && options.isBrowserActionClicked == true){
        //if browser action is clicked
        latestNotify = true;
    }else{
        latestNotify = await browser.aboutConfigPrefs.getPref("enable.floorp.updater.latest");
    }
    var APP_ID = await browser.aboutConfigPrefs.getPref("update.id.floorp");
    var displayVersion = await new Promise(resolve => {
        browser.BrowserInfo.getDisplayVersion()
        .then(data => {
            resolve(data);
        })
    });
    console.log("Floorp Display version: "+ displayVersion);
    console.log("enable.floorp.updater.latest = " + latestNotify);
    console.log("floorp.update.id = " + APP_ID);

    fetch(`${API_END_POINT}?name=${APP_ID}`)
    .then(res =>{
        if(res.ok){
            return res.json()
        }
    }).then(data =>{
        if(data.build != displayVersion){
            Notify(data.file, displayVersion, data.build);
            console.log("notificationTitle");
        }

        else if(data.build == displayVersion && latestNotify){
            NotifyNew();
            console.log("notificationTitle-last");
        }
    }).then(e =>{
        return e;
    });
};

/* Currently unnecessary code.
function initializePageAction(tab) {
    if (typeof tab.url != 'undefined' && tab.url.length ) {
        browser.pageAction.show(tab.id);
    }
}
browser.tabs.query({})
    .then((tabs) => {
        for (let tab of tabs) {
            initializePageAction(tab);
        }
    });
*/

CheckUpdate(); //Check for updates at startup.

/*Detects browserAction clicks.*/
browser.browserAction.onClicked.addListener(function(){
    CheckUpdate({isBrowserActionClicked: true});
});
