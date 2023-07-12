/*
1: ダウンロード開始だけ通知
2: ダウンロード完了だけ通知
3: ダウンロード開始と完了、両方通知
*/
const DOWNLOAD_NOTIFICATION_PREF = "floorp.download.notification";

browser.downloads.onCreated.addListener(async(file) => {
    let pref = String(await browser.aboutConfigPrefs.getPref(DOWNLOAD_NOTIFICATION_PREF));
    if (pref === "1" || pref === "3") {
        browser.notifications.create({
          "type": "basic",
          "iconUrl": "chrome://branding/content/about-logo.png",
          "title": chrome.i18n.getMessage("start"),
          "message": file.filename
        });
    }
});

browser.downloads.onChanged.addListener(async(file) => {
      let pref = String(await browser.aboutConfigPrefs.getPref(DOWNLOAD_NOTIFICATION_PREF));
      if (file.state.current == "complete" && (pref === "2" || pref === "3")) {
        let download = await browser.downloads.search({ id: file.id });
        browser.notifications.create({
          "type": "basic",
          "iconUrl": "chrome://branding/content/about-logo.png",
          "title": chrome.i18n.getMessage("finish"),
          "message": download[0].filename
        });
    }
});
