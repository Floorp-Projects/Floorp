/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
function setVerticalTabs() {
  if (Services.prefs.getIntPref("floorp.tabbar.style") == 2) {
    Services.prefs.setBoolPref("floorp.browser.tabs.verticaltab", true);
    window.setTimeout(() => {
      const verticalTabs = document.querySelector(".toolbar-items");
      verticalTabs.id = "toolbar-items-verticaltabs";
  
      const sidebarBox = document.getElementById("sidebar-box");
      //sidebarBox.style.setProperty("overflow-y", "scroll", "important")
  
      //init vertical tabs
      sidebarBox.insertBefore(verticalTabs, sidebarBox.firstChild);
      const tabBrowserArrowScrollBox = document.getElementById("tabbrowser-arrowscrollbox");
      verticalTabs.setAttribute("align", "start");
      tabBrowserArrowScrollBox.setAttribute("orient", "vertical");
      tabBrowserArrowScrollBox.removeAttribute("overflowing");
      tabBrowserArrowScrollBox.removeAttribute("scrolledtostart");
      tabBrowserArrowScrollBox.disabled = true;
      document.getElementById("tabbrowser-tabs").setAttribute("orient", "vertical");
      tabBrowserArrowScrollBox.shadowRoot.querySelector(`[part="scrollbox"]`).setAttribute("orient", "vertical");
  
      //move menubar
      document.getElementById("titlebar").before(document.getElementById("toolbar-menubar"));
      if (sidebarBox.getAttribute("hidden") === "true") {
        SidebarUI.toggle();
      }
    }, 500);

    function checkBrowserIsStartup() {
      const browserWindows = Services.wm.getEnumerator("navigator:browser");
    
      while (browserWindows.hasMoreElements()) {
        if (browserWindows.getNext() !== window) {
          return;
        }
      }
    
      SessionStore.promiseInitialized.then(() => {
        window.setTimeout(setWorkspaceLabel, 1500);
        window.setTimeout(setWorkspaceLabel, 3000);
      });
    }
    checkBrowserIsStartup();

    function setWorkspaceLabel() {
      const workspaceButton = document.getElementById("workspace-button");
      const customizeTarget = document.getElementById("nav-bar-customization-target");
    
      if (!workspaceButton) {
        console.info("Workspace button not found");
        return;
      }
    
      customizeTarget.before(workspaceButton);
    }

    //toolbar modification
    var Tag = document.createElement("style");
    Tag.id = "verticalTabsStyle"
    Tag.textContent = `@import url("chrome://browser/content/browser-verticaltabs.css");`;
    document.head.appendChild(Tag);

    Services.prefs.setIntPref("floorp.browser.tabbar.settings", 2);

    if(document.getElementById("floorp-vthover") == null && Services.prefs.getBoolPref("floorp.verticaltab.hover.enabled")){
     Tag = document.createElement("style");
     Tag.innerText = `@import url(chrome://browser/skin/options/native-verticaltab-hover.css)`;
     Tag.setAttribute("id", "floorp-vthover");
     document.head.appendChild(Tag);
    }
    //add context menu
    let target = document.getElementById("TabsToolbar-customization-target");
    target.setAttribute("context", "toolbar-context-menu");

  } else {
    Services.prefs.setBoolPref("floorp.browser.tabs.verticaltab", false);
    document.querySelector("#verticalTabsStyle")?.remove()
    let verticalTabs = document.querySelector("#toolbar-items-verticaltabs")
    if(verticalTabs != null){
      document.querySelector("#TabsToolbar").insertBefore(verticalTabs,
        document.querySelectorAll(`#TabsToolbar > .titlebar-spacer`)[1])
      let tabBrowserArrowScrollBox = document.getElementById("tabbrowser-arrowscrollbox");
      let tabsBase = document.querySelector("#tabbrowser-tabs")
      verticalTabs.setAttribute("align", "end");
      verticalTabs.removeAttribute("id")
      tabBrowserArrowScrollBox.setAttribute("orient", "horizontal");
      tabsBase.setAttribute("orient", "horizontal");
      let tabsToolBar = document.querySelector("#tabbrowser-tabs")
      tabsToolBar.style.removeProperty("--tab-overflow-pinned-tabs-width")
      tabsToolBar.removeAttribute("positionpinnedtabs")
      
      window.setTimeout(() => {
        if(tabBrowserArrowScrollBox.getAttribute("overflowing") != "true") {tabsBase.removeAttribute("overflow")}
        tabsToolBar.removeAttribute("positionpinnedtabs")
        for(let elem of document.querySelectorAll(`#tabbrowser-arrowscrollbox > tab[style*="margin-inline-start"]`)){
          elem.style.removeProperty("margin-inline-start")
        }
      },1000)
      tabBrowserArrowScrollBox.setAttribute("scrolledtostart","true")
      tabBrowserArrowScrollBox.removeAttribute("disabled");
      //sidebarBox.style.removeProperty("overflow-y")

      //move workspace button
      function moveToDefaultSpace() {
        const workspaceButton = document.getElementById("workspace-button");
        if (!workspaceButton) {
          console.error("Workspace button not found");
          return;
        }
        document.querySelector(".toolbar-items").before(workspaceButton);
      }
      moveToDefaultSpace();

      document.getElementById("floorp-vthover")?.remove();

      //remove context menu
      let target = document.getElementById("TabsToolbar-customization-target");
      target.removeAttribute("context");
    }
  }

}
setVerticalTabs()
Services.prefs.addObserver("floorp.tabbar.style", setVerticalTabs);

Services.prefs.addObserver("floorp.tabbar.style", function(){
  if(Services.prefs.getIntPref("floorp.tabbar.style") == 2){
    Services.prefs.setIntPref(tabbarContents.tabbarDisplayStylePref, 2);
  } else {
    Services.prefs.setIntPref(tabbarContents.tabbarDisplayStylePref, 0);
  }
});
