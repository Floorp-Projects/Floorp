/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 function bosshascomming() {
  if(Services.prefs.getBoolPref("floorp.browser.rest.mode", false)){
    var Tag = document.createElement("style");
    Tag.innerText = `*{display:none !important;}`
    document.getElementsByTagName("head")[0].insertAdjacentElement('beforeend',Tag);
    Tag.setAttribute("id", "none");

    gBrowser.selectedTab.toggleMuteAudio();
    reloadAllOtherTabs();
  
    var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
    .getService(Components.interfaces.nsIPromptService);
  
    let l10n = new Localization(["browser/browser.ftl"], true);
    prompts.alert(null, l10n.formatValueSync("rest-mode"),
    l10n.formatValueSync("rest-mode-description"));
  
    document.getElementById("none").remove();
   }
  }
  
  function reloadAllOtherTabs() {
    let ourTab = BrowserWindowTracker.getTopWindow().gBrowser.selectedTab;
    BrowserWindowTracker.orderedWindows.forEach(win => {
      let otherGBrowser = win.gBrowser;
      for (let tab of otherGBrowser.tabs) {
        if (tab == ourTab) {
          continue;
        }
        if (tab.pinned || tab.selected) {
          otherGBrowser.reloadTab(tab);
        } else {
          otherGBrowser.discardBrowser(tab);
        }
      }
    });
    for (let notification of document.querySelectorAll(".reload-tabs")) {
      notification.hidden = true;
    }
  }

  function OpenChromeDirectory() {
    let currProfDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let profileDir = currProfDir.path;
    let nsLocalFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
    new nsLocalFile(profileDir,).reveal();
  }

  function restartbrowser(){
  Services.obs.notifyObservers(null, "startupcache-invalidate");

    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");

    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  }

/*---------------------------------------------------------------- browser manager sidebar ----------------------------------------------------------------*/

   function displayIcons() {
     document.getElementById("sidebar2-back").hidden = false;
     document.getElementById("sidebar2-forward").hidden = false;
     document.getElementById("sidebar2-reload").hidden = false;
   }
   function hideIcons() {
     document.getElementById("sidebar2-back").hidden = true;
     document.getElementById("sidebar2-forward").hidden = true;
     document.getElementById("sidebar2-reload").hidden = true;
   }

   async function setTreeStyleTabURL() {
     const sidebar2elem = document.getElementById("sidebar2");

     const { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
     let addon = await AddonManager.getAddonByID("treestyletab@piro.sakura.ne.jp");
     let option = await addon.optionsURL;
     let sidebarURL = option.replace("options/options.html", "sidebar/sidebar.html")
     window.setTimeout(() => {
       sidebar2elem.setAttribute("src", sidebarURL);
     }, 500);
   }

   function setSidebarMode() {
    if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
      const pref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined)
      const sidebar2elem = document.getElementById("sidebar2");
      switch (pref) {
      default:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/places.xhtml");
       break;
      case 1:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/bookmarksSidebar.xhtml");
       break;
      case 2:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/historySidebar.xhtml");
       break;
      case 3:
        sidebar2elem.setAttribute("src", "about:downloads");
       break;
      case 4:
        setTreeStyleTabURL();
       break;
      case 5:
       sidebar2elem.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl", undefined));
       break;
    }
  }
}
   function changeSidebarVisibility() {
     let sidebar2 = document.getElementById("sidebar2");
     let siderbar2header = document.getElementById("sidebar2-header");
     let sidebarsplit2 = document.getElementById("sidebar-splitter2");
     let sidebarsplit3 = document.getElementById("sidebar-splitter3");

    if (sidebarsplit2.getAttribute("hidden") == "true" || sidebarsplit3.getAttribute("hidden") == "true") {
      sidebar2.style.minWidth = "10em"; 
      sidebar2.style.maxWidth = ""; 
      siderbar2header.style.display = "";
      sidebarsplit2.setAttribute("hidden", "false");
      sidebarsplit3.setAttribute("hidden", "false");
      displayIcons();
    } else {
      sidebar2.style.minWidth = "0"; 
      sidebar2.style.maxWidth = "0"; 
      siderbar2header.style.display = "none";
      sidebarsplit2.setAttribute("hidden", "true");
      sidebarsplit3.setAttribute("hidden", "true");
      hideIcons();
    }
  }

  function ViewBrowserManagerSidebar() {
    if (document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
      changeSidebarVisibility();
    }
  }

  function backSidebarSite() {
    document.getElementById("sidebar2").goBack();  //戻る
  }
  function forwardSidebarSite() {
    document.getElementById("sidebar2").goForward();  //進む
  }
  function reloadSidebarSite() {
    document.getElementById("sidebar2").reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);  //リロード
  }
  function muteSidebarSite() {
    document.getElementById("sidebar2").mute();  //ミュート
  }
  function unmuteSidebarSite() {
    document.getElementById("sidebar2").unmute();  //ミュート解除
  }

  function setBrowserManagerSidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 0);
    ViewBrowserManagerSidebar();
  }
  function setBookmarksSidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 1);
    ViewBrowserManagerSidebar();
  }
  function setHistorySidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 2);
    ViewBrowserManagerSidebar();
  }
  function setDownloadsSidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 3);
    ViewBrowserManagerSidebar();
  }
  function setTreeStyleTabSidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 4);
    ViewBrowserManagerSidebar();
  }
  function setCustomSidebarMode() {
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 5);
    ViewBrowserManagerSidebar();
  }


/*---------------------------------------------------------------- design ----------------------------------------------------------------*/

function setBrowserDesign() {
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
 }

 /*---------------------------------------------------------------- URLbar recalculation ----------------------------------------------------------------*/

 function URLbarrecalculation(){
  setTimeout(function() {
      gURLBar._updateLayoutBreakoutDimensions();
    }, 100);
    setTimeout(function() {
      gURLBar._updateLayoutBreakoutDimensions();
    }, 500);
 } 

 /*---------------------------------------------------------------- Tabbar ----------------------------------------------------------------*/
 function setTabbarMode(){
   const CustomCssPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings");
   //hide tabbrowser
    switch(CustomCssPref){
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
         window.setTimeout(function(){document.getElementById("titlebar").before(document.getElementById("toolbar-menubar"));}, 2000);
        break;
   //tabs_on_bottom
       case 4:
         var Tag = document.createElement("style");
         Tag.setAttribute("id", "tabbardesgin");
         Tag.innerText = `@import url(chrome://browser/skin/customcss/tabs_on_bottom.css);`
         document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend',Tag);
        break;
    // 5 has been removed. v10.3.0
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