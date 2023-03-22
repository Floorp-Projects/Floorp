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
  document.querySelector("#tabbrowser-arrowscrollbox")
    .shadowRoot
    .querySelector("[part=scrollbox]")
    .removeAttribute("style");

  let rowValue = Services.prefs.getIntPref("floorp.browser.tabbar.multirow.max.row");
  let tabHeight = document.querySelector(".tabbrowser-tab").clientHeight;
  document.querySelector("#tabbrowser-arrowscrollbox")
    .shadowRoot
    .querySelector("[part=scrollbox]")
    .setAttribute(
      "style", 
      Services.prefs.getBoolPref("floorp.browser.tabbar.multirow.max.enabled") ?
        ("max-height: " + tabHeight*rowValue + "px !important;") :
        ""
    );
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
    if (Services.prefs.getBoolPref("floorp.enable.multitab")) {
      setMultirowTabMaxHeight();
    }
  }, 3000);
  
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.max.row",setMultirowTabMaxHeight);
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.max.enabled",setMultirowTabMaxHeight);
  Services.prefs.addObserver("floorp.browser.tabbar.multirow.newtab-inside.enabled",setNewTabInTabs)
  
  Services.prefs.addObserver("floorp.enable.multitab", function(){
    if (Services.prefs.getBoolPref("floorp.enable.multitab")) {
      setBrowserDesign();
      setTimeout(setMultirowTabMaxHeight, 3000);
    } else {
      removeMultirowTabMaxHeight();
      setBrowserDesign();
    }
  });
}, { once: true });

//-------------------------------------------------------------------------NewTabOpenPosition----------------------------------------------------------------------------

//copy from browser.js (./browser/base/content/browser.js)
BrowserOpenTab = function ({ event, url = BROWSER_NEW_TAB_URL } = {}) {
  let relatedToCurrent = false; //"relatedToCurrent" decide where to open the new tab. Default work as last tab (right side). Floorp use this.
  let where = "tab";
  let _OPEN_NEW_TAB_POSITION_PREF = Services.prefs.getIntPref(
    "floorp.browser.tabs.openNewTabPosition"
  );

  switch (_OPEN_NEW_TAB_POSITION_PREF) {
    case 0:
      // Open the new tab as unrelated to the current tab.
      relatedToCurrent = false;
      break;
    case 1:
      // Open the new tab as related to the current tab.
      relatedToCurrent = true;
      break;
    default:
     if (event) {
      where = whereToOpenLink(event, false, true);
      switch (where) {
        case "tab":
        case "tabshifted":
          // When accel-click or middle-click are used, open the new tab as
          // related to the current tab.
          relatedToCurrent = true;
          break;
        case "current":
          where = "tab";
          break;
        }
     }
  }

  //Wrote by Mozilla(Firefox)
  // A notification intended to be useful for modular peformance tracking
  // starting as close as is reasonably possible to the time when the user
  // expressed the intent to open a new tab.  Since there are a lot of
  // entry points, this won't catch every single tab created, but most
  // initiated by the user should go through here.
  //
  // Note 1: This notification gets notified with a promise that resolves
  //         with the linked browser when the tab gets created
  // Note 2: This is also used to notify a user that an extension has changed
  //         the New Tab page.
  Services.obs.notifyObservers(
    {
      wrappedJSObject: new Promise(resolve => {
        openTrustedLinkIn(url, where, {
          relatedToCurrent,
          resolveOnNewTabCreated: resolve,
        });
      }),
    },
    "browser-open-newtab-start"
  );
}
