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

 case 6:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/tabbar_on_window_bottom.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag); 
      
      var script = document.createElement("script");
      script.setAttribute("id", "tabbar-script");
      script.src = "chrome://browser/skin/customcss/tabbar_on_window_bottom.js"; 
      document.head.appendChild(script);         
    break;
 }
 
//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------

 Services.prefs.addObserver("floorp.browser.tabbar.settings", function(){
   const CustomCssPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings")
   document.getElementById("tabbardesgin").remove();

   try {
    document.getElementById("tabbar-script").remove();
    document.getElementById("navigator-toolbox").insertBefore(document.getElementById("titlebar"), document.getElementById("navigator-toolbox").firstChild);
   } catch(e) {
   }

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

    case 6:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/tabbar_on_window_bottom.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag); 
      var script = document.createElement("script");
      script.setAttribute("id", "tabbar-script");
      script.src = "chrome://browser/skin/customcss/tabbar_on_window_bottom.js"; 
      document.head.appendChild(script);         
      break;
    } 
  }   
)