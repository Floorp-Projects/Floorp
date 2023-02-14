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

 Services.prefs.addObserver("floorp.chrome.theme.mode", function(){
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
const BROWSER_SETED_USERAGENT_PREF = "floorp.browser.UserAgent";
const GENERAL_USERAGENT_OVERRIDE_PREF = "general.useragent.override";
{
  let setUserAgent = function(BROWSER_SETED_USERAGENT) {
    switch (BROWSER_SETED_USERAGENT){
      case 0:
        Services.prefs.clearUserPref(GENERAL_USERAGENT_OVERRIDE_PREF);
        break;
      case 1:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");
        break;
      case 2:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, "Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");
        break;
      case 3:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");
        break;
      case 4:
        Services.prefs.setStringPref(GENERAL_USERAGENT_OVERRIDE_PREF, "Mozilla/5.0 (iPhone; CPU iPhone OS 16_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/108.0.5359.52 Mobile/15E148 Safari/604.1");
        break;
    // case 5 is blank value. because 5 is custom UA.
    }
  }

  let BROWSER_SETED_USERAGENT = Services.prefs.getIntPref(BROWSER_SETED_USERAGENT_PREF);
  setUserAgent(BROWSER_SETED_USERAGENT);

  Services.prefs.addObserver(BROWSER_SETED_USERAGENT_PREF, function() {
    let BROWSER_SETED_USERAGENT = Services.prefs.getIntPref(BROWSER_SETED_USERAGENT_PREF);
    setUserAgent(BROWSER_SETED_USERAGENT);
  })
}
