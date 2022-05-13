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

window.onload = () =>{
    (async() => {

        var i = await browser.aboutConfigPrefs.getPref("enable.floorp.updater.latest")
        var APP_ID = await browser.aboutConfigPrefs.getPref("update.id.floorp")
        var u = await browser.aboutConfigPrefs.getPref("enable.floorp.update")

    browser.BrowserInfo.getDisplayVersion()
    .then(data => {
        pref = data;
        console.log("Floorp Display version "+ pref)
})
        console.log("enable.floorp.updater.letest =" + i)
        console.log("floorp.update.id =" + APP_ID)

            fetch(`${API_END_POINT}?name=${APP_ID}`)
            .then(res =>{
                if(res.ok){
                    return res.json()
                }
            }).then(data =>{
                if(u == false){
                    console.log("return false stop.");
                }
                else if(data.build != pref){
                    Notify(data.file, pref, data.build);
                    console.log("notificationTitle");
                }

                else if(data.build = pref && i!=false){
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