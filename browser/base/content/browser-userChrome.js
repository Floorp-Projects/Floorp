/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
 );

//内臓されている userChrome.js をロード

//権限の問題で macOS では無効化
if (AppConstants.platform != "macosx") {
var firefox = document.createElement('script');
firefox.src = "chrome://userchromejs/content/chromecss.uc.js"; 
document.head.appendChild(firefox); 
}

if (Services.prefs.getBoolPref("floorp", false)) {    
var firefox = document.createElement('script');
firefox.src = "chrome://userchromejs/content/sidebarautohide.uc.js"; 
document.head.appendChild(firefox);
firefox.setAttribute("id", "firefox");
}