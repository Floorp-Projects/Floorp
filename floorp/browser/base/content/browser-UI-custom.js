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
