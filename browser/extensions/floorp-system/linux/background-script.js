 function initializePageAction(tab) {
       if (typeof tab.url != 'undefined' && tab.url.length ) {
           browser.pageAction.show(tab.id);
       }}
  var gettingAllTabs = browser.tabs.query({});
    gettingAllTabs.then((tabs) => {
        for (let tab of tabs) {
          initializePageAction(tab);
  }});

 let downloads = {};
  browser.downloads.onCreated.addListener(
    async function checkfordownload(file) {
    let pref = await browser.aboutConfigPrefs.getPref("floorp.download.notification");
    if(pref===1||pref===3){
      browser.notifications.create({
        "type": "basic",
        "iconUrl": "chrome://branding/content/about-logo.png",
        "title": chrome.i18n.getMessage("start"),
        "message": file.filename
    });}
    downloads[file.id] = file.filename;
});
  
 browser.downloads.onChanged.addListener(
    async function checkfordownloadfinish(file) {
    let pref = await browser.aboutConfigPrefs.getPref("floorp.download.notification");
    if (file.state.current == "complete"&& pref===2 || pref===3) {
        browser.notifications.create({
            "type": "basic",
            "iconUrl": "chrome://branding/content/about-logo.png",
            "title": chrome.i18n.getMessage("finish"),
            "message": downloads[file.id]
 });}});
