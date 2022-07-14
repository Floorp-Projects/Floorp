/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

// Floorp のデザイン設定を処理
let floorpinterfacenum = Services.prefs.getIntPref("floorp.browser.user.interface")

let ThemeCSS = {
ProtonfixUI : `@import url(chrome://browser/skin/protonfix/protonfix.css);`,
PhotonUI : `@import url(chrome://browser/skin/photon/photonChrome.css);
            @import url(chrome://browser/skin/photon/photonContent.css);`,
PhotonUIMultitab : `@import url(chrome://browser/skin/photon/photonChrome-multitab.css);
                    @import url(chrome://browser/skin/photon/photonContent-multitab.css);`,
MaterialUI : `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);`,
fluentUI : `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
gnomeUI :`@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
leptonUI: `@import url(chrome://browser/skin/lepton/userChrome.css);
           @import url(chrome://browser/skin/lepton/userContent.css);`,
}

switch(floorpinterfacenum){

//ProtonUI 
case 1: 
  var Tag = document.createElement("style");
  Tag.setAttribute("id", "browserdesgin");
  document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  Services.prefs.setIntPref("browser.uidensity", 0);
break;

//ProtonUIFix
  case 2 : 
   var Tag = document.createElement('style');
     Tag.setAttribute("id", "browserdesgin");
     Tag.innerText = ThemeCSS.ProtonfixUI;
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     Services.prefs.setIntPref("browser.uidensity", 1);
    break;
   
  case 3:
   if(!Services.prefs.getBoolPref("floorp.enable.multitab", false)){
    var Tag = document.createElement('style');
    Tag.setAttribute("id", "browserdesgin");
    Tag.innerText = ThemeCSS.PhotonUI;
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
   }
    else {
     var Tag = document.createElement('style')
     Tag.setAttribute("id", "browserdesgin");
       Tag.innerText = ThemeCSS.PhotonUIMultitab;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
    }

    break;

  case 4:
    var Tag = document.createElement('style');
    Tag.setAttribute("id", "browserdesgin");
    Tag.innerText = ThemeCSS.MaterialUI;
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);

     if (Services.prefs.getBoolPref("floorp.enable.multitab", false)) {
      var Tag = document.createElement('style');
      Tag.innerText =`
     .tabbrowser-tab {
       margin-top: 0.7em !important;
       position: relative !important;
       top: -0.34em !important;
     }
     `
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     }
    break;

  case 5:
    if (AppConstants.platform != "linux") {
    var Tag = document.createElement('style');
    Tag.setAttribute("id", "browserdesgin");
    Tag.innerText = ThemeCSS.fluentUI;
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
    }
    break;

  case 6:
     if (AppConstants.platform == "linux"){
     var Tag = document.createElement('style');
     Tag.setAttribute("id", "browserdesgin");
     Tag.innerText = ThemeCSS.gnomeUI;
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     }
    break;

  case 7:
     var Tag = document.createElement('style');
     Tag.setAttribute("id", "browserdesgin");
     Tag.innerText = ThemeCSS.leptonUI;
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     break;

}

//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------

Services.prefs.addObserver("floorp.browser.user.interface", function(){


 function recallurlbarsize(){
          setTimeout(function() {
              gURLBar._updateLayoutBreakoutDimensions();
            }, 100);
  }
  let ThemeCSS = {
    ProtonfixUI : `@import url(chrome://browser/skin/protonfix/protonfix.css);`,
    PhotonUI : `@import url(chrome://browser/skin/photon/photonChrome.css);
                @import url(chrome://browser/skin/photon/photonContent.css);`,
    PhotonUIMultitab : `@import url(chrome://browser/skin/photon/photonChrome-multitab.css);
                        @import url(chrome://browser/skin/photon/photonContent-multitab.css);`,
    MaterialUI : `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);`,
    fluentUI : `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI :`@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    leptonUI: `@import url(chrome://browser/skin/lepton/userChrome.css);
               @import url(chrome://browser/skin/lepton/userContent.css);`,
    }

  const desginPref = Services.prefs.getIntPref("floorp.browser.user.interface")
  var browserid = document.getElementById("browserdesgin");
  browserid.remove(); 

  switch(desginPref){
     case 1: 
     var Tag = document.createElement("style");
     Tag.setAttribute("id", "browserdesgin");
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     Services.prefs.setIntPref("browser.uidensity", 0);
     break;
  
     case 2:      
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.ProtonfixUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      Services.prefs.setIntPref("browser.uidensity", 1);
     break;
  
     case 3:

      if(!Services.prefs.getBoolPref("floorp.enable.multitab", false)){
       var Tag = document.createElement('style');
       Tag.setAttribute("id", "browserdesgin");
       Tag.innerText = ThemeCSS.PhotonUI;
       document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      }
       else {
        var Tag = document.createElement('style')
        Tag.setAttribute("id", "browserdesgin");
          Tag.innerText = ThemeCSS.PhotonUIMultitab;
         document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
       }  
    break;

   case 4:
      var Tag = document.createElement('style');
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.MaterialUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
  
       if (Services.prefs.getBoolPref("floorp.enable.multitab", false)) {
        var Tag = document.createElement('style');
        Tag.innerText =`
       .tabbrowser-tab {
         margin-top: 0.7em !important;
         position: relative !important;
         top: -0.34em !important;
       }
       `
       document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      }
    break;
  
    case 5:
    if (AppConstants.platform != "linux") {
    var Tag = document.createElement('style');
    Tag.setAttribute("id", "browserdesgin");
    Tag.innerText = ThemeCSS.fluentUI;
    document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
    }
    break;

  case 6:
     if (AppConstants.platform == "linux"){
     var Tag = document.createElement('style');
     Tag.setAttribute("id", "browserdesgin");
     Tag.innerText = ThemeCSS.gnomeUI;
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     }
    break;

  case 7:
     var Tag = document.createElement('style');
     Tag.setAttribute("id", "browserdesgin");
     Tag.innerText = ThemeCSS.leptonUI;
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
     break;
   } 
   recallurlbarsize()
 }   
)