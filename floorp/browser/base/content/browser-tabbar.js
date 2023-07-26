/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-------------------------------------------------------------------------Tabbar----------------------------------------------------------------------------

function setTabbarMode() {
  const tabbarCSS = {
    HideTabBrowser: "@import url(chrome://browser/skin/tabbar/hide-tabbrowser.css);",
    VerticalTab: "@import url(chrome://browser/skin/tabbar/verticaltab.css);",
    BottomTabs: "@import url(chrome://browser/skin/tabbar/tabs_on_bottom.css);",
    WindowBottomTabs: "@import url(chrome://browser/skin/tabbar/tabbar_on_window_bottom.css);"
  }
  const tabbarPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings");
  var Tag = document.createElement("style");
  Tag.setAttribute("id", "tabbardesgin");
  switch (tabbarPref) {
    case 1:
      //hide tabbrowser
      Tag.innerText = tabbarCSS.HideTabBrowser
      document.head.insertAdjacentElement('beforeend', Tag);
      break;
    case 2:
      // vertical tab CSS
      Tag.innerText = tabbarCSS.VerticalTab
      document.head.insertAdjacentElement('beforeend', Tag);
      window.setTimeout(function () {
        document.getElementById("titlebar").before(document.getElementById("toolbar-menubar"));
      }, 500);
      break;
    case 3:
      //tabs_on_bottom
      Tag.innerText = tabbarCSS.BottomTabs
      document.head.insertAdjacentElement('beforeend', Tag);
      break;
    case 4:
      Tag.innerText = tabbarCSS.WindowBottomTabs
      document.head.insertAdjacentElement('beforeend', Tag);
      var script = document.createElement("script");
      script.setAttribute("id", "tabbar-script");
      script.src = "chrome://browser/skin/tabbar/tabbar_on_window_bottom.js";
      document.head.appendChild(script);
      break;
  }
}

document.addEventListener("DOMContentLoaded", () => {
  setTabbarMode();
  Services.prefs.addObserver("floorp.browser.tabbar.settings", function () {
    document.getElementById("tabbardesgin")?.remove();
    document.getElementById("tabbar-script")?.remove();
    document.getElementById("navigator-toolbox").insertBefore(document.getElementById("titlebar"), document.getElementById("navigator-toolbox").firstChild);
    document.getElementById("TabsToolbar").before(document.getElementById("toolbar-menubar"));
  
    setTabbarMode();
  });
}, { once: true });

//-------------------------------------------------------------------------Multirow-tabs----------------------------------------------------------------------------

function setMultirowTabMaxHeight() {
  let arrowscrollbox = document.querySelector("#tabbrowser-arrowscrollbox");
  let scrollbox = arrowscrollbox.shadowRoot.querySelector("[part=scrollbox]");

  arrowscrollbox.style.maxHeight = "";
  scrollbox.removeAttribute("style");

  const isMultiRowTabEnabled = Services.prefs.getBoolPref("floorp.browser.tabbar.multirow.max.enabled");
  const rowValue = Services.prefs.getIntPref("floorp.browser.tabbar.multirow.max.row");
    const tabHeight = document.querySelector(".tabbrowser-tab").clientHeight;
    arrowscrollbox
      .style.cssText += "max-height: unset !important;";
  if (isMultiRowTabEnabled && Services.prefs.getIntPref("floorp.tabbar.style") == 1) {
    scrollbox.setAttribute("style", `max-height: ${tabHeight * rowValue}px !important;`);
  }else{
    scrollbox.setAttribute("style", `max-height: unset !important;`);
  }
  
}

function removeMultirowTabMaxHeight() {
  document.querySelector("#tabbrowser-arrowscrollbox")
    .shadowRoot
    .querySelector("[part=scrollbox]")
    .removeAttribute("style");
}

function setNewTabInTabs(){
  if(Services.prefs.getBoolPref("floorp.browser.tabbar.multirow.newtab-inside.enabled")){
    document.querySelector("#tabs-newtab-button").style.display = "initial"
    document.querySelector("#new-tab-button").style.display = "none"
  }else{
    document.querySelector("#tabs-newtab-button").style.display = ""
    document.querySelector("#new-tab-button").style.display = ""
  }
}

document.addEventListener("DOMContentLoaded", () => {
  window.setTimeout(function(){
    setNewTabInTabs()
    if (Services.prefs.getIntPref("floorp.tabbar.style") == 1 || Services.prefs.getIntPref("floorp.tabbar.style") == 2) {
      setMultirowTabMaxHeight();
    }
  }, 3000);
  
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.max.row",setMultirowTabMaxHeight);
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.max.enabled",setMultirowTabMaxHeight);
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.newtab-inside.enabled",setNewTabInTabs)
  

  let applyMultitab = () => {
    if (Services.prefs.getIntPref("floorp.tabbar.style") == 1 || Services.prefs.getIntPref("floorp.tabbar.style") == 2) {
      setBrowserDesign();
      setTimeout(setMultirowTabMaxHeight, 3000);
    } else {
      removeMultirowTabMaxHeight();
      setBrowserDesign();
      let tabToolbarItems = document.querySelector("#TabsToolbar > .toolbar-items");
      let tabsToolbar = document.getElementById("TabsToolbar-customization-target");
      let tabbrowserTabs = document.getElementById("tabbrowser-tabs");
      tabToolbarItems.style.visibility = "hidden";
      window.setTimeout(function(){
        new Promise(function() {
          tabsToolbar.setAttribute("flex", "");
          tabbrowserTabs.setAttribute("style", "-moz-box-flex: unset !important;");
          setTimeout(function() {
            tabsToolbar.setAttribute("flex", "1");
            tabbrowserTabs.style.removeProperty("-moz-box-flex");[
              tabToolbarItems.style.visibility = ""
            ]
          }, 0);
        })
      }, 1000);
    }
  }
  Services.prefs.addObserver("floorp.tabbar.style", applyMultitab);

  const tabs = document.querySelector(`#tabbrowser-tabs`)
  tabs.on_wheel = (event) => {
    if (Services.prefs.getBoolPref("toolkit.tabbox.switchByScrolling")) {
      if (event.deltaY > 0 != Services.prefs.getBoolPref("floorp.tabscroll.reverse")) {
        tabs.advanceSelectedTab(1, Services.prefs.getBoolPref("floorp.tabscroll.wrap"));
      } else {
        tabs.advanceSelectedTab(-1, Services.prefs.getBoolPref("floorp.tabscroll.wrap"));
      }
      event.preventDefault();
      event.stopPropagation();
    }
  }
}, { once: true })
