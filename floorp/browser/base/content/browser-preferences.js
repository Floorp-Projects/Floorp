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

const BROWSER_SETED_USERAGENT = Services.prefs.getIntPref("floorp.browser.UserAgent");
 switch (BROWSER_SETED_USERAGENT){
    case 0:
      Services.prefs.clearUserPref("general.useragent.override");
     break;
    case 1:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
    case 2:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (Macintosh; Intel Mac OS X 12_6) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
    case 3:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/105.0.5195.129 Mobile/15E148 Safari/604.1");
     break;
    case 4:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
}

Services.prefs.addObserver("floorp.browser.UserAgent", function(){
  const BROWSER_SETED_USERAGENT = Services.prefs.getIntPref("floorp.browser.UserAgent");
 switch (BROWSER_SETED_USERAGENT){
    case 0:
      Services.prefs.clearUserPref("general.useragent.override");
     break;
    case 1:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
    case 2:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (Macintosh; Intel Mac OS X 12_6) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
    case 3:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
     break;
    case 4:
      Services.prefs.setStringPref("general.useragent.override", "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/105.0.5195.129 Mobile/15E148 Safari/604.1");
     break;
    default:
      Services.prefs.clearUserPref("general.useragent.override");
     break;
  }
 }   
)