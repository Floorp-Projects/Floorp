/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
    Services.prefs.addObserver("floorp.browser.sidebar2.mode", function(){
     setSidebarMode();
 })};

 const SIDEBAR_AMOUNT = 20 /* Amount of sidebar elements with customurl. From 1 to 20  */

 if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
	for (let sbar_id = 1; sbar_id <= SIDEBAR_AMOUNT; sbar_id++) {
		Services.prefs.addObserver(`floorp.browser.sidebar2.customurl${sbar_id}`, function() {
			setSidebarMode()
			setCustomURLFavicon(sbar_id)
			setSidebarIconView()
		})
	}
}

//startup functions
setSidebarIconView();
setSidebarMode();
setAllfavicons();

if(Services.prefs.getBoolPref("floorp.browser.restore.sidebar.panel", false)) {
 setSidebarMode();
}else{
 changeSidebarVisibility();
}