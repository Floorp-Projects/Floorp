/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function enableRestMode() {
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
  const webpanel = document.getElementById("TST");
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
    const webpanel_id = modeValuePref - DEFAULT_STATIC_SIDEBAR_MODES_AMOUNT /* Get sidebar_id */
    const sidebar2elem = document.getElementById("sidebar2");
    const wibpanel_usercontext = Services.prefs.getIntPref(`floorp.browser.sidebar2.customurl${webpanel_id}.usercontext`, undefined);
    const panelWidth = Services.prefs.getIntPref(`floorp.browser.sidebar2.width.mode${modeValuePref}`, undefined);

    if(panelWidth !== "" || panelWidth !== undefined || panelWidth !== null){
      document.getElementById("sidebar2-box").setAttribute("width", panelWidth);
    }

    switch (modeValuePref) {
      case 0:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/places.xhtml");
        showSidebarNodes(0);
        break;
      case 1:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/bookmarksSidebar.xhtml");
        showSidebarNodes(0);
        break;
      case 2:
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/historySidebar.xhtml");
        showSidebarNodes(0);
        break;
      case 3:
        sidebar2elem.setAttribute("src", "about:downloads");
        showSidebarNodes(0);
        break;
      case 4:
        setTreeStyleTabURL();
        showSidebarNodes(1);
        break;
      default:
        showSidebarNodes(2);
         if(document.getElementById(`webpanel${webpanel_id}`) == null){
          let browserManagerSidebarWebpanel = document.createXULElement("browser");
          browserManagerSidebarWebpanel.setAttribute("usercontextid", wibpanel_usercontext);
          browserManagerSidebarWebpanel.setAttribute("xmlns", "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");
          browserManagerSidebarWebpanel.setAttribute("id", `webpanel${webpanel_id}`);
          browserManagerSidebarWebpanel.setAttribute("class", "webpanels");
          browserManagerSidebarWebpanel.setAttribute("flex", "1");
          browserManagerSidebarWebpanel.setAttribute("autoscroll", "false");
          browserManagerSidebarWebpanel.setAttribute("disablehistory", "true");
          browserManagerSidebarWebpanel.setAttribute("disablefullscreen", "true");
          browserManagerSidebarWebpanel.setAttribute("style", "min-width:13em;");
          browserManagerSidebarWebpanel.setAttribute("tooltip", "aHTMLTooltip");
          browserManagerSidebarWebpanel.setAttribute("disableglobalhistory", "true");
          browserManagerSidebarWebpanel.setAttribute("messagemanagergroup", "webext-browsers");
          browserManagerSidebarWebpanel.setAttribute("context", "contentAreaContextMenu");
          browserManagerSidebarWebpanel.setAttribute("webextension-view-type", "sidebar");
          browserManagerSidebarWebpanel.setAttribute("autocompletepopup", "PopupAutoComplete");
          browserManagerSidebarWebpanel.setAttribute("initialBrowsingContextGroupId", "40");
          browserManagerSidebarWebpanel.setAttribute("type", "content");
          browserManagerSidebarWebpanel.setAttribute("remote", "true");
          browserManagerSidebarWebpanel.setAttribute("maychangeremoteness", "true");
          browserManagerSidebarWebpanel.setAttribute("src", Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${webpanel_id}`, undefined));
          document.getElementById("sidebar2-box").appendChild(browserManagerSidebarWebpanel);
        break;
        } else {
          document.getElementById(`webpanel${webpanel_id}`).setAttribute("src", Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${webpanel_id}`, undefined));
        }
    }
  }
}

function changeSidebarVisibility() {
  let sidebar2 = document.getElementById("sidebar2");
  let siderbar2header = document.getElementById("sidebar2-header");
  let sidebarsplit2 = document.getElementById("sidebar-splitter2");
  let sidebar2box = document.getElementById("sidebar2-box");

  if (sidebarsplit2.getAttribute("hidden") == "true" || sidebarsplit2.getAttribute("hidden") == "true") {
    sidebar2.style.minWidth = "15em";
    sidebar2.style.maxWidth = "";
    sidebar2box.style.minWidth = "15em";
    sidebar2box.style.maxWidth = "";
    siderbar2header.style.display = "";
    sidebarsplit2.setAttribute("hidden", "false");
    displayIcons();
  } else {
    sidebar2.style.minWidth = "0";
    sidebar2.style.maxWidth = "0";
    sidebar2box.style.minWidth = "0";
    sidebar2box.style.maxWidth = "0";
    siderbar2header.style.display = "none";
    sidebarsplit2.setAttribute("hidden", "true");
    hideIcons();
    removeAttributeSelectedNode();
  }
}

function displayBrowserManagerSidebar() {
  if (document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
    changeSidebarVisibility();
  }
}

function setCustomURLFavicon(sbar_id) {
    var sbar_url = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${sbar_id}`)
    document.getElementById(`select-CustomURL${sbar_id}`).style.listStyleImage = `url(http://www.google.com/s2/favicons?domain=${sbar_url})`;

    const wibpanel_usercontext = Services.prefs.getIntPref(`floorp.browser.sidebar2.customurl${sbar_id}.usercontext`, undefined);
    if(wibpanel_usercontext != 0){
      document.getElementById(`select-CustomURL${sbar_id}`).style.borderLeft = `solid 2px var(--usercontext-color-${wibpanel_usercontext})`;
    }else{
      document.getElementById(`select-CustomURL${sbar_id}`).style.borderLeft = "";
    }
}

/* Function responsible for action on the sidebar
	0 - Back
	1 - Forward
	2 - Reload
	3 - Goto Index
*/
function sidebarSiteAction(action){
  const modeValuePref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined)
  const webpanel_id = modeValuePref - DEFAULT_STATIC_SIDEBAR_MODES_AMOUNT
  let webpanel = document.getElementById(`webpanel${webpanel_id}`)
  switch (action) {
    case 0:
		webpanel.goBack() /* Go backwards */
		break
	case 1:
		webpanel.goForward() /* Go forward */
		break
	case 2:
		webpanel.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE) /* Reload */
		break;
	case 3:
		webpanel.gotoIndex() /* Goto Index */
		break
  }
}

/* From 0 to 4 - StaticModeSetter. 5 modes, start from 0
0 - Browser Manager
1 - Bookmark
2 - History
3 - Downloads
4 - TreeStyleTab
*/ 
function setStaticSidebarMode(sbar_id) { 
    if(Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined) != sbar_id || document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true"){
      Services.prefs.setIntPref("floorp.browser.sidebar2.mode", sbar_id);
      displayBrowserManagerSidebar();
      setSelectedPanel();
    } else {
      changeSidebarVisibility();
    }
} 

/* From 0 to 19 - CustomURLSetter. 20 URLs in total.*/
function setDynamicSidebarMode(sbar_id){
  let custom_url_id = sbar_id + DEFAULT_STATIC_SIDEBAR_MODES_AMOUNT /* Hack to start from 0, yet maintain compatibility with the previous solution. eg: If 0, than 5. If 1, than 6  */

  if(Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined) != custom_url_id || document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true"){
    Services.prefs.setIntPref("floorp.browser.sidebar2.mode", custom_url_id);
    displayBrowserManagerSidebar();
    setSelectedPanel();
  } else {
    changeSidebarVisibility();
  }
}

function setSidebarIconView() {
  for (let sbar_id = 0; sbar_id <= DEFAULT_DYNAMIC_CUSTOMURL_MODES_AMOUNT; sbar_id++) {
    var sbar_url = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${sbar_id}`, undefined)
    document.getElementById(`select-CustomURL${sbar_id}`).hidden = (sbar_url != "") ? false : true
  }
}

function keepSidebar2boxWidth() {
  const pref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode");
  Services.prefs.setIntPref(`floorp.browser.sidebar2.width.mode${pref}`, document.getElementById("sidebar2-box").width);
}

function getSelectedNode(){
  let selectedMode = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined);
  let selectedNode = document.querySelector(`.sidepanel-icon[panel="${selectedMode}"]`);
  return selectedNode;
}

function removeAttributeSelectedNode(){
  let Nodes = document.getElementsByClassName("sidepanel-icon");

  for(let i = 0; i < Nodes.length; i++){
    Nodes[i].setAttribute("checked", "false");
  }
}


function setAllfavicons() {
  for (let sbar_id = 0; sbar_id <= DEFAULT_DYNAMIC_CUSTOMURL_MODES_AMOUNT; sbar_id++) {
    var sbar_favicon = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${sbar_id}`)
    document.getElementById(`select-CustomURL${sbar_id}`).style.listStyleImage = `url(http://www.google.com/s2/favicons?domain=${sbar_favicon}`;

    var wibpanel_usercontext = Services.prefs.getIntPref(`floorp.browser.sidebar2.customurl${sbar_id}.usercontext`, undefined);
    if(wibpanel_usercontext != 0){
      document.getElementById(`select-CustomURL${sbar_id}`).style.borderLeft = `solid 2px var(--usercontext-color-${wibpanel_usercontext})`;
    }else{
      document.getElementById(`select-CustomURL${sbar_id}`).style.borderLeft = "";
    }
  }
}

function setSelectedPanel() {
    let selectedMode = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined);
    let selectedNode = document.querySelector(`.sidepanel-icon[panel="${selectedMode}"]`);
    removeAttributeSelectedNode();
    selectedNode.setAttribute("checked", "true");
}

function showSidebarNodes(sidebar_mode) { /* Managers - 0; TST - 1  webpanel - 2*/
    var sbar_css = ""
    let webpanels = document.getElementsByClassName("webpanels");
    const modeValuePref = Services.prefs.getIntPref("floorp.browser.sidebar2.mode", undefined);
    const webpanel_id = modeValuePref - DEFAULT_STATIC_SIDEBAR_MODES_AMOUNT;
    const selectedwebpanel = document.getElementById(`webpanel${webpanel_id}`);

    if (document.getElementById("sidebar2style")){document.getElementById("sidebar2style").remove()}
    var Tag = document.createElement("style")
    if (sidebar_mode == 0) { 
        sbar_css = "#TST{max-height:0 !important;}#sidebar2-reload, #sidebar2-go-index, #sidebar2-forward,#sidebar2-back{display:none !important;}"
    } else if(sidebar_mode == 1){
        sbar_css = "#sidebar2{max-height:0 !important;}#sidebar2-reload,#sidebar2-go-index, #sidebar2-forward,#sidebar2-back{display:none !important;}"
    } else {
      sbar_css = "#TST, #sidebar2{max-height:0 !important;}"
    }

    Tag.innerText = sbar_css
    document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend", Tag);
    Tag.setAttribute("id", "sidebar2style");

    for (let i = 0; i < webpanels.length; i++) {
        webpanels[i].hidden = true;
    }
    if(selectedwebpanel != null){selectedwebpanel.hidden = false;}
}

 function BSMcontextmenu(event){
   clickedWebpanel = event.explicitOriginalTarget.id;
   webpanel = clickedWebpanel.replace("select-CustomURL", "webpanel");
   contextWebpanel = document.getElementById(webpanel);
   needLoadedWebpanel = document.getElementsByClassName("needLoadedWebpanel");

   if(contextWebpanel == null){
      for (let i = 0; i < needLoadedWebpanel.length; i++) {
        needLoadedWebpanel[i].disabled = true;
      }
   } else {
      for (let i = 0; i < needLoadedWebpanel.length; i++) {
        needLoadedWebpanel[i].disabled = false;
      }
    }
  }

function UnloadWebpanel() {
  let sidebarsplit2 = document.getElementById("sidebar-splitter2");

  Services.prefs.setIntPref("floorp.browser.sidebar2.mode", 30);

  if (sidebarsplit2.getAttribute("hidden") != "true") {
    changeSidebarVisibility();
  }
  contextWebpanel.remove();
}

 function muteSidebar(){
    if(contextWebpanel.audioMuted == false){
      contextWebpanel.mute();
    } else {
     contextWebpanel.unmute();
   }  
  }

/*---------------------------------------------------------------- design ----------------------------------------------------------------*/

function setBrowserDesign() {
  let floorpinterfacenum = Services.prefs.getIntPref("floorp.browser.user.interface")
  const ThemeCSS = {
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
      break;
    case 2:
      var Tag = document.createElement('style');
      Tag.setAttribute("id", "browserdesgin");
      Tag.innerText = ThemeCSS.ProtonfixUI;
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
    case 3:
      if (!Services.prefs.getBoolPref("floorp.enable.multitab", false)) {
        var Tag = document.createElement('style');
        Tag.setAttribute("id", "browserdesgin");
        Tag.innerText = ThemeCSS.PhotonUI;
        document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      } else {
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
