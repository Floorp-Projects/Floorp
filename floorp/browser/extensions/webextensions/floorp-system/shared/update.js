(async () => {
    const API_END_POINT = "https://floorp-update.ablaze.one/browser/latest.json"

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
                browser.notifications.onClosed.removeListener(handle_onClosed);
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

        let displayVersion = await browser.BrowserInfo.getDisplayVersion();

        let platformInfo = await browser.runtime.getPlatformInfo();

        console.log("Floorp Display version: "+ displayVersion);
        console.log("enable.floorp.updater.latest = " + latestNotifyEnabled);

        fetch(API_END_POINT)
            .then(res =>{
                if (!res.ok) throw `${res.status} ${res.statusText}`;
                return res.json();
            }).then(datas => {
                let platformKeyName =
                    platformInfo["os"] === "mac" ?
                        "mac" :
                        `${platformInfo["os"]}-${platformInfo["arch"]}`;
                let data = datas[platformKeyName];
                if (!data["notify"]) return;
                if (data["version"] !== displayVersion) {
                    Notify(data["url"], displayVersion, data["version"]);
                    console.log("notificationTitle");
                } else if (latestNotifyEnabled) {
                    NotifyNew();
                    console.log("notificationTitle-last");
                }
            }).catch(e =>{
                console.error(e);
            });
    };

    let isPortable = false;
    try {
        isPortable = await browser.aboutConfigPrefs.getBoolPref("floorp.isPortable");
    } catch (e) {}
    if (isPortable) return;

    CheckUpdate();

    if (browser.browserAction) {
        browser.browserAction.onClicked.addListener(function(){
            CheckUpdate({isBrowserActionClicked: true});
        });
    }
})();
