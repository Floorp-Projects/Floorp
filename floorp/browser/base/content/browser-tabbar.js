/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with This
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-------------------------------------------------------------------------Tabbar----------------------------------------------------------------------------

let tabbarContents = {}

const tabbarDisplayStyleFunctions = {
  init() {
    tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", Services.prefs.getIntPref(tabbarContents.tabbarDisplayStylePref));
    tabbarContents.tabbarWindowManageContainer.id = "floorp-tabbar-window-manage-container";
  },

  setTabbarDisplayStyle() {
    tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", Services.prefs.getIntPref(tabbarContents.tabbarDisplayStylePref));

    switch (Services.prefs.getIntPref(tabbarContents.tabbarDisplayStylePref)){
      default:
        //default style
        tabbarDisplayStyleFunctions.revertToDefaultStyle();
        tabbarContents.navigatorToolboxtabbarElement.setAttribute("floorp-tabbar-display-style", "0");
        break;
      case 1:
        tabbarDisplayStyleFunctions.revertToDefaultStyle();

        //hide tabbar
        tabbarContents.modifyCSS = document.createElement("style");
        tabbarContents.modifyCSS.id = "floorp-tabbar-modify-css";
        tabbarContents.modifyCSS.textContent = `
          #TabsToolbar-customization-target {
            display: none !important;
          }
        `;
        document.querySelector("head").appendChild(tabbarContents.modifyCSS);
        tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", "1");
        break;
      
      case 2:
        tabbarDisplayStyleFunctions.revertToDefaultStyle();

        //optimize vertical tabbar
        tabbarContents.tabbarElement.setAttribute("hidden", "true");
        tabbarContents.navbarElement.appendChild(document.querySelector("#floorp-tabbar-window-manage-container"));
        tabbarContents.modifyCSS = document.createElement("style");
        tabbarContents.modifyCSS.id = "floorp-tabbar-modify-css";
        tabbarContents.modifyCSS.textContent = `
          #toolbar-menubar > .titlebar-buttonbox-container {
            display: none !important;
          }
          #titlebar {
            appearance: none !important;
          }
        `;
        document.querySelector("head").appendChild(tabbarContents.modifyCSS);
        tabbarDisplayStyleFunctions.setWorkspaceLabelToNavbar();
        tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", "1");
        break;
      case 3:
        tabbarDisplayStyleFunctions.revertToDefaultStyle();

        //move tabbar to navigator-toolbox's bottom
        tabbarContents.navigatorToolboxtabbarElement.appendChild(tabbarContents.tabbarElement);
        tabbarContents.navbarElement.appendChild(document.querySelector("#floorp-tabbar-window-manage-container"));
        tabbarContents.modifyCSS = document.createElement("style");
        tabbarContents.modifyCSS.id = "floorp-tabbar-modify-css";
        tabbarContents.modifyCSS.textContent = `
          #toolbar-menubar > .titlebar-buttonbox-container {
            display: none !important;
          }
        `;
        document.querySelector("head").appendChild(tabbarContents.modifyCSS);
        tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", "2");
        break;
      case 4:
        tabbarDisplayStyleFunctions.revertToDefaultStyle();

        //move tabbar to window's bottom
        tabbarContents.browserElement.after(tabbarContents.titleBarElement);
        tabbarContents.navbarElement.appendChild(document.querySelector("#floorp-tabbar-window-manage-container"));
        tabbarContents.modifyCSS = document.createElement("style");
        tabbarContents.modifyCSS.id = "floorp-tabbar-modify-css";
        tabbarContents.modifyCSS.textContent = `
          #toolbar-menubar > .titlebar-buttonbox-container {
            display: none !important;
          }
        `;
        document.querySelector("head").appendChild(tabbarContents.modifyCSS);
        tabbarContents.tabbarElement.setAttribute("floorp-tabbar-display-style", "3");
        break;
    }
  },

  revertToDefaultStyle() {
    tabbarContents.tabbarElement.removeAttribute("floorp-tabbar-display-style");
    tabbarContents.tabbarElement.removeAttribute("hidden");
    tabbarContents.tabbarElement.appendChild(document.querySelector("#floorp-tabbar-window-manage-container"));
    tabbarContents.titleBarElement.appendChild(tabbarContents.tabbarElement);
    tabbarContents.navigatorToolboxtabbarElement.prepend(tabbarContents.titleBarElement);
    document.querySelector("#floorp-tabbar-modify-css")?.remove();
    tabbarContents.tabbarElement.removeAttribute("floorp-tabbar-display-style");
    tabbarDisplayStyleFunctions.moveToDefaultSpace();
  },

  setWorkspaceLabelToNavbar() {
    //move workspace button
    let workspaceButton = document.getElementById("workspace-button");
    let customizeTarget = document.getElementById("nav-bar-customization-target");

    if (workspaceButton == null) {
      console.error("Workspace button not found");
      return;
    }

    customizeTarget.before(workspaceButton);
  },

  moveToDefaultSpace() {
    let workspaceButton = document.getElementById("workspace-button");
    if (workspaceButton == null) {
      console.error("Workspace button not found");
      return;
    }
    document.querySelector(".toolbar-items").before(workspaceButton);
  }
}

SessionStore.promiseInitialized.then(() => {
  tabbarContents = {
    tabbarDisplayStylePref: "floorp.browser.tabbar.settings",
    tabbarElement: document.querySelector("#TabsToolbar"),
    titleBarElement: document.querySelector("#titlebar"),
    navbarElement: document.querySelector("#nav-bar"),
    tabbarWindowManageContainer: document.querySelector("#TabsToolbar > .titlebar-buttonbox-container"),
    navigatorToolboxtabbarElement: document.querySelector("#navigator-toolbox"),
    browserElement: document.querySelector("#browser"),
    modifyCSS: null,
  }  

  //run
  tabbarDisplayStyleFunctions.init();
  tabbarDisplayStyleFunctions.setTabbarDisplayStyle();
    
  //listen
  Services.prefs.addObserver(tabbarContents.tabbarDisplayStylePref, tabbarDisplayStyleFunctions.setTabbarDisplayStyle);
});

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
