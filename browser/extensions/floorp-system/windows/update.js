const API_END_POINT = "https://repo.ablaze.one/api/"

const Notify = (url) =>{
    const msg = browser.i18n;
    browser.notifications.create({
        "type": "basic",
        "iconUrl": browser.runtime.getURL("icons/floorp.png"),
        "title": msg.getMessage("notificationTitle"),
        "message": msg.getMessage("notificationContent")
    });
    browser.notifications.onClicked.addListener(() =>{
        window.location.href = url
    });
};

const CheckUpdate = async () =>{
    const APP_ID = await browser.aboutConfigPrefs.getPref("update.id.floorp");
    console.log(APP_ID)
    const displayVersion = await new Promise(resolve => {
        browser.BrowserInfo.getDisplayVersion()
        .then(data => {
            resolve(data);
        })
    });

    fetch(`${API_END_POINT}?name=${APP_ID}`)
    .then(res =>{
        if(res.ok){
            return res.json()
        }
     }).then(data =>{
        if(data.build != displayVersion){
            Notify();
        }
    }).then(e =>{
        return e;
    });
};

CheckUpdate(); 
