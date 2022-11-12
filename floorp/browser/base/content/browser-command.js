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

    let l10n = new Localization(["browser/floorp.ftl"], true);
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

function setSidebarWidth(webpanel_id){
  if (webpanel_id != "" && BROWSER_SIDEBAR_DATA.index.indexOf(webpanel_id) != -1) {

    const panelWidth = BROWSER_SIDEBAR_DATA.data[webpanel_id].width ?? Services.prefs.getIntPref("floorp.browser.sidebar2.global.webpanel.width", undefined);

    document.getElementById("sidebar2-box").setAttribute("width", panelWidth);
}
}

function setSidebarMode() {
    const webpanel_id = Services.prefs.getStringPref("floorp.browser.sidebar2.page", "");
  if (Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false) && webpanel_id != "" && BROWSER_SIDEBAR_DATA.index.indexOf(webpanel_id) != -1) {

    setSidebarPageSetting(webpanel_id)
  }
}

function setSidebarPageSetting(webpanel_id){
  const webpanelURL = BROWSER_SIDEBAR_DATA.data[webpanel_id].url
    const sidebar2elem = document.getElementById("sidebar2");
    const wibpanel_usercontext = BROWSER_SIDEBAR_DATA.data[webpanel_id].usercontext ?? 0
    const webpanel_userAgent = BROWSER_SIDEBAR_DATA.data[webpanel_id].userAgent ?? false


setSidebarWidth(webpanel_id)

    switch (webpanelURL) {
      case "floorp//bmt":
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/places.xhtml");
        showSidebarNodes(0);
        break;
      case "floorp//bookmarks":
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/bookmarksSidebar.xhtml");
        showSidebarNodes(0);
        break;
      case "floorp//history":
        sidebar2elem.setAttribute("src", "chrome://browser/content/places/historySidebar.xhtml");
        showSidebarNodes(0);
        break;
      case "floorp//downloads":
        sidebar2elem.setAttribute("src", "about:downloads");
        showSidebarNodes(0);
        break;
      case "floorp//tst":
        setTreeStyleTabURL();
        showSidebarNodes(1);
        break;
      default:
        showSidebarNodes(2);
         if(document.getElementById(`webpanel${webpanel_id}`) != null && document.getElementById(`webpanel${webpanel_id}`).getAttribute("usercontextid") != wibpanel_usercontext) document.getElementById(`webpanel${webpanel_id}`).remove()
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
          browserManagerSidebarWebpanel.setAttribute("context", "");
          browserManagerSidebarWebpanel.setAttribute("webextension-view-type", "sidebar");
          browserManagerSidebarWebpanel.setAttribute("autocompletepopup", "PopupAutoComplete");
          browserManagerSidebarWebpanel.setAttribute("initialBrowsingContextGroupId", "40");
          browserManagerSidebarWebpanel.setAttribute("type", "content");
          browserManagerSidebarWebpanel.setAttribute("remote", "true");
          browserManagerSidebarWebpanel.setAttribute("changeuseragent", String(webpanel_userAgent));
          browserManagerSidebarWebpanel.setAttribute("maychangeremoteness", "true");
          browserManagerSidebarWebpanel.setAttribute("src", webpanelURL);
          document.getElementById("sidebar2-box").appendChild(browserManagerSidebarWebpanel);
        break;
        } else {
          document.getElementById(`webpanel${webpanel_id}`).setAttribute("src", webpanelURL);
          if(document.getElementById(`webpanel${webpanel_id}`).getAttribute("changeuseragent") != String(webpanel_userAgent) || document.getElementById(`webpanel${webpanel_id}`).getAttribute("usercontextid") != wibpanel_usercontext){
            document.getElementById(`webpanel${webpanel_id}`).setAttribute("usercontextid", wibpanel_usercontext);
            document.getElementById(`webpanel${webpanel_id}`).setAttribute("changeuseragent", String(webpanel_userAgent));
            document.getElementById(`webpanel${webpanel_id}`).reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE)
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
    let sData = BROWSER_SIDEBAR_DATA.data[sbar_id.slice(7)]
    var sbar_url = sData.url
    if(sbar_url.slice(0,7) != "floorp//") document.getElementById(`${sbar_id}`).style.listStyleImage = `url(http://www.google.com/s2/favicons?domain=${sbar_url})`;

    const wibpanel_usercontext = sData.usercontext ?? 0
    const container_list = ContextualIdentityService.getPublicIdentities()
    if(wibpanel_usercontext != 0 && container_list.findIndex(e => e.userContextId === wibpanel_usercontext) != -1){
      let  container_color = container_list[container_list.findIndex(e => e.userContextId === wibpanel_usercontext)].color
      document.getElementById(`${sbar_id}`).style.borderLeft = `solid 2px ${container_color == "toolbar" ? "var(--toolbar-field-color)" : container_color}`;
    }else{
      document.getElementById(`${sbar_id}`).style.borderLeft = "";
    }
}

/* Function responsible for action on the sidebar
	0 - Back
	1 - Forward
	2 - Reload
	3 - Goto Index
*/
function sidebarSiteAction(action){
  const modeValuePref = Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined)
  let webpanel = document.getElementById(`webpanel${modeValuePref}`)
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
/* From 0 to 19 - CustomURLSetter. 20 URLs in total.*/
function setSidebarPage(){
  let custom_url_id = event.target.id.replace("select-", "")

  if(Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined) != custom_url_id || document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true"){
    Services.prefs.setStringPref("floorp.browser.sidebar2.page", custom_url_id);
    displayBrowserManagerSidebar();
    setSelectedPanel();
  } else {
    changeSidebarVisibility();
  }
}

function setSidebarIconView() {
  for(let elem of BROWSER_SIDEBAR_DATA.index){
    if(document.getElementById(`select-${elem}`) == null){
      let sidebarItem = document.createXULElement("toolbarbutton");
      sidebarItem.id = `select-${elem}`;
      sidebarItem.classList.add("sidepanel-icon")
      sidebarItem.classList.add("sicon-list")
      sidebarItem.setAttribute("oncommand","setSidebarPage()")
      if(BROWSER_SIDEBAR_DATA.data[elem]["url"].slice(0,8) == "floorp//"){
        if(BROWSER_SIDEBAR_DATA.data[elem]["url"] in STATIC_SIDEBAR_L10N_LIST) sidebarItem.setAttribute("data-l10n-id",STATIC_SIDEBAR_L10N_LIST[BROWSER_SIDEBAR_DATA.data[elem].url])
      }else{
      sidebarItem.classList.add("webpanel-icon")
sidebarItem.setAttribute("context","webpanel-context")
      }

      let sidebarItemImage = document.createXULElement("image");
      sidebarItemImage.classList.add("toolbarbutton-icon")
      sidebarItem.appendChild(sidebarItemImage)

      let sidebarItemLabel = document.createXULElement("label");
      sidebarItemLabel.classList.add("toolbarbutton-text")
sidebarItemLabel.setAttribute("crop","right")
sidebarItemLabel.setAttribute("flex","1")
      sidebarItem.appendChild(sidebarItemLabel)

      document.getElementById("sidebar-select-box").insertBefore(sidebarItem,document.getElementById("bsb_spacer"));
    }else{
      document.getElementById("sidebar-select-box").insertBefore(document.getElementById(`select-${elem}`),document.getElementById("bsb_spacer"));
    }
  }
  let siconAll = document.querySelectorAll(".sicon-list")
  let sicon = siconAll.length
  let side = BROWSER_SIDEBAR_DATA.index.length
  if(sicon > side){
    for(let i = 0;i < (sicon - side);i++){
      siconAll[i].remove()
    }
  }
}

function keepSidebar2boxWidth() {
  const pref = Services.prefs.getStringPref("floorp.browser.sidebar2.page");
  BROWSER_SIDEBAR_DATA.data[pref].width = document.getElementById("sidebar2-box").width
  Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA) );
}

function keepSidebar2boxWidthForGlobal() {
  Services.prefs.setIntPref("floorp.browser.sidebar2.global.webpanel.width", document.getElementById("sidebar2-box").width);
}

function getSelectedNode(){
  let selectedMode = Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined);
  let selectedNode = document.querySelector(`#select-${selectedMode}`);
  return selectedNode;
}

function removeAttributeSelectedNode(){
  let Nodes = document.getElementsByClassName("sidepanel-icon");

  for(let i = 0; i < Nodes.length; i++){
    Nodes[i].setAttribute("checked", "false");
  }
}


function setAllfavicons() {
  for (let elem of document.querySelectorAll(".webpanel-icon")) {
    setCustomURLFavicon(elem.id);
  }
}

function setSelectedPanel() {
  let selectedMode = Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined);
  let selectedNode = document.querySelector(`#select-${selectedMode}`);
    removeAttributeSelectedNode();
    selectedNode.setAttribute("checked", "true");
}

function showSidebarNodes(sidebar_mode) { /* Managers - 0; TST - 1  webpanel - 2*/
    var sbar_css = ""
    let webpanels = document.getElementsByClassName("webpanels");
    const modeValuePref = Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined);
    const selectedwebpanel = document.getElementById(`webpanel${modeValuePref}`);

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
   webpanel = clickedWebpanel.replace("select-", "webpanel");
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

  Services.prefs.setStringPref("floorp.browser.sidebar2.page", "");

  if (sidebarsplit2.getAttribute("hidden") != "true") {
    changeSidebarVisibility();
  }
  contextWebpanel.remove();
}

function changeWebpanelUA() {
  BROWSER_SIDEBAR_DATA.data[clickedWebpanel.replace("select-", "")].userAgent = ((document.getElementById(clickedWebpanel.replace("select-", "webpanel")).getAttribute("changeuseragent") ?? "false") == "false")

  Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));

  //reload webpanel
  sidebarSiteAction(2);
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
  let updateNumberDate = new Date()
  let updateNumber = `${updateNumberDate.getFullYear()}${updateNumberDate.getMonth()}${updateNumberDate.getDate()}${updateNumberDate.getHours()}${updateNumberDate.getMinutes()}${updateNumberDate.getSeconds()}`
  const ThemeCSS = {
    ProtonfixUI: `@import url(chrome://browser/skin/protonfix/protonfix.css);`,
    LeptonUI:    `@import url(chrome://browser/skin/lepton/userChrome.css?${updateNumber});
                   @import url(chrome://browser/skin/lepton/userChrome.css?${updateNumber});`,
    LeptonUIMultitab: `@import url(chrome://browser/skin/lepton/photonChrome-multitab.css?${updateNumber});
                    @import url(chrome://browser/skin/lepton/photonContent-multitab.css?${updateNumber});`,
    MaterialUI: `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);`,
    MaterialUIMultitab: `@import url(chrome://browser/skin/floorplegacy/floorplegacy.css);
    .tabbrowser-tab { margin-top: 0.7em !important;  position: relative !important;  top: -0.34em !important; }`,
    fluentUI: `@import url(chrome://browser/skin/fluentUI/fluentUI.css);`,
    gnomeUI: `@import url(chrome://browser/skin/gnomeUI/gnomeUI.css);`,
    FluerialUI: `@import url(chrome://browser/skin/floorplegacy/test_legacy.css);`,
  }
  var Tag = document.createElement('style');
  Tag.setAttribute("id", "browserdesgin");
  switch (floorpinterfacenum) {
    //ProtonUI 
    case 1:
      break;
    case 2:
      Tag.innerText = ThemeCSS.ProtonfixUI;
      break;
    case 3:
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.LeptonUIMultitab : ThemeCSS.LeptonUI; ;
      break;
    case 4:
      Tag.innerText = Services.prefs.getBoolPref("floorp.enable.multitab", false) ? ThemeCSS.MaterialUIMultitab : ThemeCSS.MaterialUI;
      break;
    case 5:
      if (AppConstants.platform != "linux") Tag.innerText = ThemeCSS.fluentUI;
      break;
    case 6:
      if (AppConstants.platform == "linux") Tag.innerText = ThemeCSS.gnomeUI;
      break;
    case 8: 
      Tag.innerText = ThemeCSS.FluerialUI;
      break;
  }
  document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
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
  const tabbarCSS = {
    HideTabBrowser: "@import url(chrome://browser/skin/tabbar/hide-tabbrowser.css);",
    VerticalTab: "@import url(chrome://browser/skin/tabbar/verticaltab.css);",
    BottomTabs: "@import url(chrome://browser/skin/tabbar/tabs_on_bottom.css);",
    WindowBottomTabs: "@import url(chrome://browser/skin/tabbar/tabbar_on_window_bottom.css);"
  }
  const tabbarPref = Services.prefs.getIntPref("floorp.browser.tabbar.settings");
  var Tag = document.createElement("style");
  Tag.setAttribute("id", "tabbardesgin");
  switch (tabbarPref) { //hide tabbrowser
    case 1:
      Tag.innerText = tabbarCSS.HideTabBrowser
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
    // vertical tab CSS
    case 2:
      Tag.innerText = tabbarCSS.VerticalTab
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      window.setTimeout(function () { document.getElementById("titlebar").before(document.getElementById("toolbar-menubar")); }, 2000);
      break;
    //tabs_on_bottom
    case 3:
      Tag.innerText = tabbarCSS.BottomTabs
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      break;
    case 4:
      Tag.innerText = tabbarCSS.WindowBottomTabs
      document.getElementsByTagName('head')[0].insertAdjacentElement('beforeend', Tag);
      var script = document.createElement("script");
      script.setAttribute("id", "tabbar-script");
      script.src = "chrome://browser/skin/tabbar/tabbar_on_window_bottom.js";
      document.head.appendChild(script);
      break;
  }
}
