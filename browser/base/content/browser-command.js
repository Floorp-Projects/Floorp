/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function bosshascomming() {
  if (Services.prefs.getBoolPref("floorp.browser.rest.mode", false)) {
    var Tag = document.createElement("style");
    Tag.innerText = `*{display:none !important;}`
    document.getElementsByTagName("head")[0].insertAdjacentElement('beforeend', Tag);
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

function restartbrowser() {
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

const sidebar_icons = ["sidebar2-back", "sidebar2-forward", "sidebar2-reload"]

function displayIcons() {
  sidebar_icons.forEach(function (sbar_icon) {
    document.getElementById(sbar_icon).hidden = false;
  })
}

function hideIcons() {
  sidebar_icons.forEach(function (sbar_icon) {
    document.getElementById(sbar_icon).hidden = true;
  })
}

async function setTreeStyleTabURL() {
  const webpanel = document.getElementById("webpanel");
  const { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
  let addon = await AddonManager.getAddonByID("treestyletab@piro.sakura.ne.jp");
  let option = await addon.optionsURL;
  let sidebarURL = option.replace("options/options.html", "sidebar/sidebar.html")
  window.setTimeout(() => {
    webpanel.setAttribute("src", sidebarURL);
  }, 50);
}

function setSidebarMode() {
  if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)) {
    const modeValuePref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined);
    const sidebar2elem = document.getElementById("sidebar2");
    const webpanel = document.getElementById("webpanel");
    const panelWidth = Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${modeValuePref}`, undefined); 
  
    if(panelWidth !== "" || panelWidth !== undefined || panelWidth !== null){
      document.getElementById("sidebar2-box").setAttribute("width", panelWidth);
    }
  
    switch (modeValuePref) {
      case -1:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/places.xhtml");
        changeBrowserManagerSidebarConfigShowBrowserManagers();
        break;
      case 0:
        sidebar2elem.setAttribute("src", "chrome://browser/content/syncedtabs/sidebar.xhtml");
        changeBrowserManagerSidebarConfigShowBrowserManagers();
        break;
      case 1:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/bookmarksSidebar.xhtml");
        changeBrowserManagerSidebarConfigShowBrowserManagers();
        break;
      case 2:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/historySidebar.xhtml");
        changeBrowserManagerSidebarConfigShowBrowserManagers();
        break;
      case 3:
        sidebar2elem.setAttribute("src", "about:downloads");
        changeBrowserManagerSidebarConfigShowBrowserManagers();
        break;
      case 4:
        setTreeStyleTabURL();
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 5:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl1", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 6:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl2", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 7:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl3", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 8:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl4", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 9:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl5", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 10:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl6", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 11:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl7", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 12:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl8", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 13:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl9", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 14:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl10", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 15:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl11", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 16:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl12", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 17:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl13", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 18:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl14", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 19:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl15", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 20:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl16", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 21:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl17", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
      case 22:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl18", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 23:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl19", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break; 
      case 24:
        webpanel.setAttribute("src", Services.prefs.getStringPref("floorp.browser.sidebar2.customurl20", undefined));
        changeBrowserManagerSidebarConfigShowWebpanels();
        break;
    }
  }
}

function changeSidebarVisibility() {
  let sidebar2 = document.getElementById("sidebar2");
  let siderbar2header = document.getElementById("sidebar2-header");
  let sidebarsplit2 = document.getElementById("sidebar-splitter2");

  if (sidebarsplit2.getAttribute("hidden") == "true" || sidebarsplit2.getAttribute("hidden") == "true") {
    sidebar2.style.minWidth = "10em";
    sidebar2.style.maxWidth = "";
    siderbar2header.style.display = "";
    sidebarsplit2.setAttribute("hidden", "false");
    displayIcons();
  } else {
    sidebar2.style.minWidth = "0";
    sidebar2.style.maxWidth = "0";
    siderbar2header.style.display = "none";
    sidebarsplit2.setAttribute("hidden", "true");
    hideIcons();
  }
}

function ViewBrowserManagerSidebar() {
  if (document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
    changeSidebarVisibility();
  }
}

function setCustomURLFavicon(sbar_id) {
  let sbar_url = Services.prefs.getStringPref("floorp.browser.sidebar2.customurl1")
  document.getElementById(`select-CustomURL${sbar_id}`).style.listStyleImage = `url(http://www.google.com/s2/favicons?domain=${sbar_url})`
}

function backSidebarSite() {
  document.getElementById("webpanel").goBack();  //戻る
}
function forwardSidebarSite() {
  document.getElementById("webpanel").goForward();  //進む
}
function reloadSidebarSite() {
  document.getElementById("webpanel").reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);  //リロード
}
function muteSidebarSite() {
  document.getElementById("webpanel").mute();  //ミュート
}
function unmuteSidebarSite() {
  document.getElementById("webpanel").unmute();  //ミュート解除
}

/* Sidebar Modes
  0 - Browser Manager Sidebar Mode
  1 - Bookmark Sidebar Mode
  2 - History Sidebar Mode
  3 - Download Sidebar Mode
  4 - Tree Style Sidebar Mode
  [5 - To the end] - Custom URL Modes
 */ 
  function setCustomSidebarMode(sidebar_mode_id) {
	Services.prefs.setIntPref("floorp.browser.sidebar2.mode", sidebar_mode_id);
	ViewBrowserManagerSidebar()
}

function setSidebarIconView() {
  for (let sbar_id = 1; sbar_id < 21; sbar_id++) {
    document.getElementById(`select-CustomURL${sbar_id}`).hidden = (Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${sbar_id}`, undefined) != "") ? false : true;
  }
}

function keepSidebar2boxWidth() {
  const pref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode");
  Services.prefs.setIntPref(`floorp.browser.sidebar2.width.mode${pref}`, document.getElementById("sidebar2-box").width);
}

/*---------------------------------------------------------------- design ----------------------------------------------------------------*/

function setBrowserDesign() {
  let floorpinterfacenum = Services.prefs.getIntPref("floorp.browser.user.interface")
  let ThemeCSS = {
    ProtonfixUI: `@import url(chrome://browser/skin/protonfix/protonfix.css);`,
    PhotonUI:    `@import url(chrome://browser/skin/photon/photonChrome.css);
                   @import url(chrome://browser/skin/photon/photonContent.css);`,
    PhotonUIMultitab: `@import url(chrome://browser/skin/photon/photonChrome-multitab.css);
                    @import url(chrome://browser/skin/photon/photonContent-multitab.css);`,
    MaterialUI: `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);`,
    fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    leptonUI: `@import url(chrome://browser/skin/lepton/userChrome.css);
                  @import url(chrome://browser/skin/lepton/userContent.css);`,
  }
  switch (floorpinterfacenum) {
    //ProtonUI 
    case 1:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "browserdesgin");
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      Services.prefs.setIntPref("browser.uidensity", 0);
      break;
    case 2:
      var Tag = document.createElement('style');
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.ProtonfixUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      Services.prefs.setIntPref("browser.uidensity", 1);
      break;
    case 3:
      if (!Services.prefs.getBoolPref("floorp.enable.multitab", false)) {
        var Tag = document.createElement('style');
        Tag.setAttribute("id", "browserdesgin");
        Tag.innerText = ThemeCSS.PhotonUI;
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      }
      else {
        var Tag = document.createElement('style')
        Tag.setAttribute("id", "browserdesgin");
        Tag.innerText = ThemeCSS.PhotonUIMultitab;
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      }
      break;
    case 4:
      var Tag = document.createElement('style');
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.MaterialUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);

      if (Services.prefs.getBoolPref("floorp.enable.multitab", false)) {
        var Tag = document.createElement('style');
        Tag.innerText = `
      .tabbrowser-tab {
        margin-top: 0.7em !important;
        position: relative !important;
        top: -0.34em !important;
     }
     `
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      }
      break;
    case 5:
      if (AppConstants.platform != "linux") {
        var Tag = document.createElement('style');
        Tag.setAttribute("id", "browserdesgin");
        Tag.innerText = ThemeCSS.fluentUI;
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      }
      break;

    case 6:
      if (AppConstants.platform == "linux") {
        var Tag = document.createElement('style');
        Tag.setAttribute("id", "browserdesgin");
        Tag.innerText = ThemeCSS.gnomeUI;
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      }
      break;
    case 7:
      var Tag = document.createElement('style');
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.leptonUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
  }
}

/*---------------------------------------------------------------- URLbar recalculation ----------------------------------------------------------------*/

function URLbarrecalculation() {
  setTimeout(function () {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 100);
  setTimeout(function () {
    gURLBar._updateLayoutBreakoutDimensions();
  }, 500);
}

/*---------------------------------------------------------------- Tabbar ----------------------------------------------------------------*/
function setTabbarMode() {
  const CustomCssPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings");
  //hide tabbrowser
  switch (CustomCssPref) {
    case 2:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/hide-tabbrowser.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
    // vertical tab CSS
    case 3:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/verticaltab.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      window.setTimeout(function () { document.getElementById("titlebar").before(document.getElementById("toolbar-menubar")); }, 2000);
      break;
    //tabs_on_bottom
    case 4:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/tabs_on_bottom.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
    // 5 has been removed. v10.3.0
    case 6:
      var Tag = document.createElement("style");
      Tag.setAttribute("id", "tabbardesgin");
      Tag.innerText = `@import url(chrome://browser/skin/customcss/tabbar_on_window_bottom.css);`
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      var script = document.createElement("script");
      script.setAttribute("id", "tabbar-script");
      script.src = "chrome://browser/skin/customcss/tabbar_on_window_bottom.js";
      document.head.appendChild(script);
      break;
  }
}

function setAllfavicons() {
  for (let sbar_id = 1; sbar_id < 20; sbar_id++) {
    var sbar_favicon = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${sbar_id}`)
    document.getElementById(`select-CustomURL${sbar_id}`).style.listStyleImage = `url(http://www.google.com/s2/favicons?domain=${sbar_favicon}`;
  }
}

function changeMuteStatus() {
  let webpanel = document.getElementById("webpanel");
  let muteicon = document.getElementById("sidebar2-mute");
  if (muteicon.getAttribute("mute") == "false") {
    webpanel.mute();
    muteicon.setAttribute("mute", "true");
  } else {
    webpanel.unmute();
    muteicon.setAttribute("mute", "false");
  }
}

function changeBrowserManagerSidebarConfigShowWebpanels() {
  if (document.getElementById("sidebar2style")){document.getElementById("sidebar2style").remove()}
  var Tag = document.createElement("style");
  Tag.innerText = `#sidebar2{max-height:0 !important;}`
  document.getElementsByTagName("head")[0].insertAdjacentElement('beforeend', Tag);
  Tag.setAttribute("id", "sidebar2style");
}

function changeBrowserManagerSidebarConfigShowBrowserManagers() {
  if (document.getElementById("sidebar2style")){document.getElementById("sidebar2style").remove()}
  var Tag = document.createElement("style");
  Tag.innerText = `#webpanel{max-height:0 !important;}#sidebar2-reload,#sidebar2-forward,#sidebar2-back{display:none !important;}`
  document.getElementsByTagName("head")[0].insertAdjacentElement('beforeend', Tag);
  Tag.setAttribute("id", "sidebar2style");
}