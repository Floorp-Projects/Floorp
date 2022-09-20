/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//import utils
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

if (Services.prefs.getBoolPref("floorp.material.effect.enable", false)) {    
  var Tag = document.createElement("style");
  Tag.innerText = `@import url(chrome://browser/skin/optioncss/micaforeveryone.css)`
  document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
  Tag.setAttribute("id", "micaforeveryone");
}

Services.prefs.addObserver("floorp.material.effect.enable", function(){
 if (Services.prefs.getBoolPref("floorp.material.effect.enable", false)) {
   var Tag = document.createElement("style");
   Tag.innerText = `@import url(chrome://browser/skin/optioncss/micaforeveryone.css)`
   document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
   Tag.setAttribute("id", "micaforeveryone");
 }
 else {
   document.getElementById("micaforeveryone").remove();
 }});

  if (Services.prefs.getBoolPref("floorp.Tree-type.verticaltab.optimization", false)) {    
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/treestyletab.css)`
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
    Tag.setAttribute("id", "treestyletabopti");
  }

  Services.prefs.addObserver("floorp.Tree-type.verticaltab.optimization", function(){
    if (Services.prefs.getBoolPref("floorp.Tree-type.verticaltab.optimization", false)) {
      var Tag = document.createElement("style");
      Tag.innerText = `@import url(chrome://browser/skin/optioncss/treestyletab.css)`
      document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
      Tag.setAttribute("id", "treestyletabopti");
    }
    else {
      document.getElementById("treestyletabopti").remove();
  }});

  if (Services.prefs.getBoolPref("floorp.optimized.msbutton.ope", false)) {    
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/msbutton.css)`
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
    Tag.setAttribute("id", "optimizedmsbuttonope");
  }
 Services.prefs.addObserver("floorp.optimized.msbutton.ope", function(){
   if (Services.prefs.getBoolPref("floorp.optimized.msbutton.ope", false)) {
     var Tag = document.createElement("style");
     Tag.innerText = `@import url(chrome://browser/skin/optioncss/msbutton.css)`
     document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
     Tag.setAttribute("id", "optimizedmsbuttonope");
   }
   else {
    document.getElementById("optimizedmsbuttonope").remove();
  }});

  if (Services.prefs.getBoolPref("floorp.bookmarks.bar.focus.mode", false)) {    
    var Tag = document.createElement("style");
    Tag.innerText = `@import url(chrome://browser/skin/optioncss/bookmarkbar_autohide.css)`
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
    Tag.setAttribute("id", "bookmarkbarfocus");
  }
   Services.prefs.addObserver("floorp.bookmarks.bar.focus.mode", function(){
     if (Services.prefs.getBoolPref("floorp.bookmarks.bar.focus.mode", false)) {
     var Tag = document.createElement("style");
     Tag.innerText = `@import url(chrome://browser/skin/optioncss/bookmarkbar_autohide.css)`
     document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
     Tag.setAttribute("id", "bookmarkbarfocus");
    }
  else {
   document.getElementById("bookmarkbarfocus").remove();
  }});


if (Services.prefs.getBoolPref("floorp.bookmarks.fakestatus.mode", false)) {
  window.setTimeout(function(){document.body.appendChild(document.getElementById('PersonalToolbar'));}, 250);
  }

 Services.prefs.addObserver("floorp.bookmarks.fakestatus.mode", function(){
   if (Services.prefs.getBoolPref("floorp.bookmarks.fakestatus.mode", false)) {
    document.getElementById("browser-bottombox").after(document.getElementById("PersonalToolbar"));
   }
   else {
    document.getElementById("nav-bar").after(document.getElementById("PersonalToolbar"));
   }
 })

  if (Services.prefs.getBoolPref("floorp.search.top.mode", false)) {    
     var Tag = document.createElement("style");
     Tag.innerText = `@import url(chrome://browser/skin/optioncss/move_page_inside_searchbar.css)` 
     document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
     Tag.setAttribute("id", "searchbartop");
  }
  Services.prefs.addObserver("floorp.search.top.mode", function(){
    if (Services.prefs.getBoolPref("floorp.search.top.mode", false)) {
      var Tag = document.createElement("style");
      Tag.innerText = `@import url(chrome://browser/skin/optioncss/move_page_inside_searchbar.css)` 
      document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
      Tag.setAttribute("id", "searchbartop");
    }
    else {
      document.getElementById("searchbartop").remove();
    }
   }
 )
 if (Services.prefs.getBoolPref("floorp.legacy.dlui.enable", false)) {    
  var Tag = document.createElement("style");
  Tag.innerText = `@import url(chrome://browser/skin/optioncss/browser-custum-dlmgr.css)`
  document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
  Tag.setAttribute("id", "dlmgrcss");
}
Services.prefs.addObserver("floorp.legacy.dlui.enable", function(){
 if (Services.prefs.getBoolPref("floorp.legacy.dlui.enable", false)) {
   var Tag = document.createElement("style");
   Tag.innerText = `@import url(chrome://browser/skin/optioncss/browser-custum-dlmgr.css)`
   document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
   Tag.setAttribute("id", "dlmgrcss");
 }
 else {
   document.getElementById("dlmgrcss").remove();
 }});

 if (Services.prefs.getBoolPref("floorp.downloading.red.color", false)) {    
  var Tag = document.createElement("style");
  Tag.innerText = `@import url(chrome://browser/skin/optioncss/downloading-redcolor.css`
  document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
  Tag.setAttribute("id", "dlredcolor");
}
Services.prefs.addObserver("floorp.downloading.red.color", function(){
 if (Services.prefs.getBoolPref("floorp.downloading.red.color", false)) {
   var Tag = document.createElement("style");
   Tag.innerText = `@import url(chrome://browser/skin/optioncss/downloading-redcolor.css`
   document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
   Tag.setAttribute("id", "dlredcolor");
 }
 else {
   document.getElementById("dlredcolor").remove();
 }});

 /*------------------------------------------- sidebar -------------------------------------------*/

 if (Services.prefs.getBoolPref("floorp.browser.sidebar.right", false)) {    
  var Tag = document.createElement("style");
  Tag.innerText = `.browser-sidebar2 {-moz-box-ordinal-group: 10 !important;}#sidebar-select-box{-moz-box-ordinal-group: 15 !important;} #sidebar-splitter2 {-moz-box-ordinal-group: 9 !important;}`;
  document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
  Tag.setAttribute("id", "sidebar2css");
}
Services.prefs.addObserver("floorp.browser.sidebar.right", function(){
  if (Services.prefs.getBoolPref("floorp.browser.sidebar.right", false)) {
    var Tag = document.createElement("style");
    Tag.innerText = `.browser-sidebar2 {-moz-box-ordinal-group: 10 !important;}#sidebar-select-box{-moz-box-ordinal-group: 15 !important;} #sidebar-splitter2 {-moz-box-ordinal-group: 9 !important;}`;
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag);
    Tag.setAttribute("id", "sidebar2css");
  }
  else {
    document.getElementById("sidebar2css").remove();
  }});

  if (!Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
      var Tag = document.createElement("style");
      Tag.innerText = `#sidebar-button2, #wrapper-sidebar-button2, .browser-sidebar2, #sidebar-select-box  {display: none !important;}`;
      document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",Tag)
  }

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

setSidebarIconView();
setSidebarMode();
setAllfavicons();
changeSidebarVisibility();