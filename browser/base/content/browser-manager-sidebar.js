/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
    Services.prefs.addObserver("floorp.browser.sidebar2.mode", function(){
     setSidebarMode();
 })};

 if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl1", function(){
    setSidebarMode();
    setCustomURL1Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl2", function(){
    setSidebarMode();
    setCustomURL2Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl3", function(){
    setSidebarMode();
    setCustomURL3Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl4", function(){
    setSidebarMode();
    setCustomURL4Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl5", function(){
    setSidebarMode();
    setCustomURL5Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl6", function(){
    setSidebarMode();
    setCustomURL6Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl7", function(){
    setSidebarMode();
    setCustomURL7Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl8", function(){
    setSidebarMode();
    setCustomURL8Favicon();
    setSidebarIconView();
 })
   Services.prefs.addObserver("floorp.browser.sidebar2.customurl9", function(){
    setSidebarMode();
    setCustomURL9Favicon();
    setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl10", function(){
   setSidebarMode();
   setCustomURL10Favicon();
   setSidebarIconView();
})
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl11", function(){
   setSidebarMode();
   setCustomURL11Favicon();
   setSidebarIconView();
})
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl12", function(){
   setSidebarMode();
   setCustomURL12Favicon();
   setSidebarIconView();
})
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl13", function(){
   setSidebarMode();
   setCustomURL13Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl14", function(){
   setSidebarMode();
   setCustomURL14Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl15", function(){
   setSidebarMode();
   setCustomURL15Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl16", function(){
   setSidebarMode();
   setCustomURL16Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl17", function(){
   setSidebarMode();
   setCustomURL17Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl18", function(){
   setSidebarMode();
   setCustomURL18Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl19", function(){
   setSidebarMode();
   setCustomURL19Favicon();
   setSidebarIconView();
 })
 Services.prefs.addObserver("floorp.browser.sidebar2.customurl20", function(){
   setSidebarMode();
   setCustomURL20Favicon();
   setSidebarIconView();
 })
};



//startup functions
setSidebarIconView();
setSidebarMode();
setAllfavicons();

if(Services.prefs.getBoolPref("floorp.browser.restore.sidebar.panel", false)) {
 setSidebarMode();
}else{
 changeSidebarVisibility();
}