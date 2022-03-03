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

window.onload = () =>{
    (async() => {
    const BROWSER_VERSION = await browser.aboutConfigPrefs.getPref("floorp.verison")
    console.log("success! Browser Verison is " + BROWSER_VERSION)
    fetch(`${API_END_POINT}?name=${APP_ID}`)
    .then(res =>{
        if(res.ok){
            return res.json()
        }
    }).then(data =>{
        if(data.build != BROWSER_VERSION){
            Notify(data.file, BROWSER_VERSION, data.build);
            console.log("notificationTitle");
        }
    }).then(e =>{
        return e;
    });
 })();
};