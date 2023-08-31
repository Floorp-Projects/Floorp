/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// I glared at the source code for about 3 hours, but for some reason I decided to use the server because it would be unclear because of the Floorp interface settings. God forgive me


const BROWSER_CHROME_SYSTEM_COLOR = Services.prefs.getIntPref("floorp.chrome.theme.mode");

switch (BROWSER_CHROME_SYSTEM_COLOR){
    case 1:
      Services.prefs.setIntPref("ui.systemUsesDarkTheme", 1);
     break;
    case 0:
      Services.prefs.setIntPref("ui.systemUsesDarkTheme", 0);
     break;
    case -1:
      Services.prefs.clearUserPref("ui.systemUsesDarkTheme");
     break;
}

 Services.prefs.addObserver("floorp.chrome.theme.mode", () => {
    const BROWSER_CHROME_SYSTEM_COLOR = Services.prefs.getIntPref("floorp.chrome.theme.mode");
      switch(BROWSER_CHROME_SYSTEM_COLOR){
        case 1:
          Services.prefs.setIntPref("ui.systemUsesDarkTheme", 1);
         break;
        case 0:
          Services.prefs.setIntPref("ui.systemUsesDarkTheme", 0);
         break;
        case -1:
          Services.prefs.clearUserPref("ui.systemUsesDarkTheme");
         break;
       }
   }   
)

// UserAgent
/*
0: Default
1: Chrome (Windows)
2: Chrome (macOS)
3: Chrome (Linux)
4: Chrome (iOS)
*/
Services.scriptloader.loadSubScript("chrome://browser/content/ua_data.js", this);
const BROWSER_SETED_USERAGENT_PREF = "floorp.browser.UserAgent";
const GENERAL_USERAGENT_OVERRIDE_PREF = "general.useragent.override";
{
  let setUserAgent = function(BROWSER_SETED_USERAGENT) {
    switch (BROWSER_SETED_USERAGENT){
      case 0:
        Services.prefs.clearUserPref(GENERAL_USERAGENT_OVERRIDE_PREF);
        break;
      case 1:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, CHROME_STABLE_UA.win);
        break;
      case 2:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, CHROME_STABLE_UA.mac);
        break;
      case 3:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, CHROME_STABLE_UA.linux);
        break;
      case 4:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, "Mozilla/5.0 (iPhone; CPU iPhone OS 16_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/110.0.5481.83 Mobile/15E148 Safari/604.1");
        break;
      case 5:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, Services.prefs.getCharPref("floorp.general.useragent.override"));
    }
  }

  let BROWSER_SETED_USERAGENT = Services.prefs.getIntPref(BROWSER_SETED_USERAGENT_PREF);
  setUserAgent(BROWSER_SETED_USERAGENT);

  Services.prefs.addObserver(BROWSER_SETED_USERAGENT_PREF, function() {
    let BROWSER_SETED_USERAGENT = Services.prefs.getIntPref(BROWSER_SETED_USERAGENT_PREF);
    setUserAgent(BROWSER_SETED_USERAGENT);
  })
}


// backup Floorp Notes Pref
var { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const FLOORP_NOTES_PREF = "floorp.browser.note.memos";
const FLOORP_NOTES_LATEST_BACKUP_TIME_PREF = "floorp.browser.note.backup.latest.time";

const backupFloorpNotes = async () => {
  const memos = Services.prefs.getCharPref(FLOORP_NOTES_PREF).slice(1, -1);
  const time = new Date().getTime();
  const backup = { [time]: memos };
  const jsonToStr = JSON.stringify(backup).slice(1, -1);

  const filePath = OS.Path.join(OS.Constants.Path.profileDir, "floorp_notes_backup.json");
  const txtEncoded = new TextEncoder().encode(`${jsonToStr},`);

  Services.prefs.setCharPref(FLOORP_NOTES_LATEST_BACKUP_TIME_PREF, time);

  if (!(await OS.File.exists(filePath))) {
    await OS.File.writeAtomic(filePath, new TextEncoder().encode(`{"data":{`));
  }

  const valOpen = await OS.File.open(filePath, { write: true, append: true });
  await valOpen.write(txtEncoded);
  await valOpen.close();
};

if(Services.prefs.getCharPref(FLOORP_NOTES_PREF) != ""){
  backupFloorpNotes();
}

function getAllBackupedNotes() {
  const filePath = OS.Path.join(OS.Constants.Path.profileDir, "floorp_notes_backup.json");
  const content = OS.File.read(filePath, { encoding: "utf-8" })
    .then(content => {
      content = content.slice(0, -1) + "}}";
      return JSON.parse(content);
    });
  return content;
}

//Backup Limit is 10.
getAllBackupedNotes().then(content => {
  const backupLimit = 10;
  const dataKeys = Object.keys(content.data);

  if (dataKeys.length > backupLimit) {
    const sortedKeys = dataKeys.sort((a, b) => b - a);
    const deleteKeys = sortedKeys.slice(backupLimit);

    deleteKeys.forEach(key => {
      delete content.data[key];
    });

    let jsonToStr = JSON.stringify(content).slice(0, -2) + ",";
    const filePath = OS.Path.join(OS.Constants.Path.profileDir, "floorp_notes_backup.json");
    OS.File.writeAtomic(filePath, new TextEncoder().encode(jsonToStr));
  }
});

// Apply Floorp user.js

const FLOORP_USERJS_PREF = "floorp.browser.userjs";

(async function applyUserJSCustomize() {
  const pref = Services.prefs.getStringPref("floorp.user.js.customize", "");

  if (pref != "") {
      const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
      const userjs = PathUtils.join(PROFILE_DIR, "user.js");

      try{userjs.remove(false);} catch(e) {}
 
      fetch(pref)
        .then(response => response.text())
        .then(async data => {
          const encoder = new TextEncoder("UTF-8");
          const writeData = encoder.encode(data);

          await IOUtils.write(userjs, writeData);
        });
  }
})();
