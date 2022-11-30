{
    const API_END_POINT = "https://repo.ablaze.one/api/"

    const Notify = async(url, now, latest) =>{
        const msg = browser.i18n;
        let created_notificationId = await browser.notifications.create({
            "type": "basic",
            "iconUrl": browser.runtime.getURL("icons/link-48.png"),
            "title": msg.getMessage("notificationTitle"),
            "message": msg.getMessage("notificationContent", [now, latest]),
        });

        let handle_onClicked = (notificationId) => {
            if (created_notificationId === notificationId) {
                browser.tabs.create({ url: url });
            }
        }
        browser.notifications.onClicked.addListener(handle_onClicked);

        let handle_onClosed = (notificationId) => {
            if (created_notificationId === notificationId) {
                browser.notifications.onClicked.removeListener(handle_onClicked);
                browser.notifications.onClosed.removeListener(handle_onClicked);
            }
        }
        browser.notifications.onClosed.addListener(handle_onClosed);
    };

    const NotifyNew = (now, latest) =>{
        const msg = browser.i18n;
        browser.notifications.create({
            "type": "basic",
            "iconUrl": browser.runtime.getURL("icons/link-48-last.png"),
            "title": msg.getMessage("notificationTitle"),
            "message": msg.getMessage("notificationContent-last", [now, latest])
        });
    };

    const CheckUpdate = async(options) =>{
        let updaterEnabled = await browser.aboutConfigPrefs.getPref("enable.floorp.update");
        if (!updaterEnabled) return; //updater is disabled

        let latestNotifyEnabledPref = await browser.aboutConfigPrefs.getPref("enable.floorp.updater.latest");
        let latestNotifyEnabled = (options && options.isBrowserActionClicked) || 
                                    latestNotifyEnabledPref;

        let APP_ID = await browser.aboutConfigPrefs.getPref("update.id.floorp");

        /*
        var displayVersion = await new Promise(resolve => {
            browser.BrowserInfo.getDisplayVersion()
            .then(data => {
                resolve(data);
            })
        });
        */
        let displayVersion = await browser.BrowserInfo.getDisplayVersion();

        console.log("Floorp Display version: "+ displayVersion);
        console.log("enable.floorp.updater.latest = " + latestNotifyEnabled);
        console.log("floorp.update.id = " + APP_ID);

        fetch(`${API_END_POINT}?name=${APP_ID}`)
            .then(res =>{
                if (!res.ok) throw `${res.status} ${res.statusText}`;
                return res.json();
            }).then(data =>{
                if(data.build !== displayVersion){
                    Notify(data.file, displayVersion, data.build);
                    console.log("notificationTitle");
                } else if (latestNotifyEnabled) {
                    NotifyNew();
                    console.log("notificationTitle-last");
                }
            }).catch(e =>{
                console.error(e);
            });
    };

    CheckUpdate();

    browser.browserAction.onClicked.addListener(function(){
        CheckUpdate({isBrowserActionClicked: true});
    });
}
