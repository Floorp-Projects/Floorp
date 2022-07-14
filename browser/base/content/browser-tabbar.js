/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

//タブバーの設定を処理。平行使用不可なため、switch で区分
const CustomCssPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings")

//hide tabbrowser
 switch(CustomCssPref){
   case 1: 
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
   break;
   case 2:
       var Tag = document.createElement("style");
       Tag.setAttribute("id", "tabbardesgin");
       Tag.innerText = `@import url(chrome://browser/skin/customcss/hide-tabbrowser.css);`
       document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
   break;
// vertical tab CSS
   case 3:
     var Tag = document.createElement("style");
     Tag.setAttribute("id", "tabbardesgin");
     Tag.innerText = `@import url(chrome://browser/skin/customcss/verticaltab.css);`
     document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
   break;
//tabs_on_bottom
 case 4:
   var Tag = document.createElement("style");
   Tag.setAttribute("id", "tabbardesgin");
   Tag.innerText = `@import url(chrome://browser/skin/customcss/tabs_on_bottom.css);`
   document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
   break;
 case 5:
    var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/classic_firefox_menu_button.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);   
    break;
 }
 
//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------

 Services.prefs.addObserver("floorp.browser.tabbar.settings", function(){
   const CustomCssPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings")
   var menuid = document.getElementById("tabbardesgin");
   menuid.remove(); 

   switch(CustomCssPref){
      case 1: 
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      break;
   
      case 2:      
       var Tag = document.createElement("style");
       Tag.setAttribute("id", "tabbardesgin");
       Tag.innerText = `@import url(chrome://browser/skin/customcss/hide-tabbrowser.css);`
       document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      break;
   
   
   // vertical tab CSS
      case 3:
        var Tag = document.createElement("style");
        Tag.setAttribute("id", "tabbardesgin");
        Tag.innerText = `@import url(chrome://browser/skin/customcss/verticaltab.css);`
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      break;
   
   
   //tabs_on_bottom
    case 4:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/tabs_on_bottom.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
      break;
   
   
    case 5:
       var Tag = document.createElement("style");
       Tag.setAttribute("id", "tabbardesgin");
       Tag.innerText = `@import url(chrome://browser/skin/customcss/classic_firefox_menu_button.css);`
       document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);   
       break;
    } 
  }   
)