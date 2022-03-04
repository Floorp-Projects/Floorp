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
        var pref = null;

    browser.BrowserInfo.getDisplayVersion()
    .then(data => {
        pref = data;
        console.log("Floorp Display version "+ pref)
})
        console.log("enable.floorp.updater.latest =" + i)
        console.log("floorp.update.id =" + APP_ID)

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