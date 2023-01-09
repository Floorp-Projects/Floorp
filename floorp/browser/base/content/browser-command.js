/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let os_languages = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;
let prefBranch = Services.prefs.getDefaultBranch(null);
prefBranch.setStringPref(
  "floorp.browser.sidebar.useIconProvider",
  os_languages.includes("zh-CN") ?
    "yandex" :
    "duckduckgo"
);

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
  }, 1250);
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
    var webpandata = BROWSER_SIDEBAR_DATA.data[webpanel_id]
    var webpanobject = document.getElementById(`webpanel${webpanel_id}`)
    const webpanelURL = webpandata.url
    const sidebar2elem = document.getElementById("sidebar2");
    const wibpanel_usercontext = webpandata.usercontext ?? 0
    const webpanel_userAgent = webpandata.userAgent ?? false
    
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
         if(webpanobject != null && webpanobject.getAttribute("usercontextid") != wibpanel_usercontext) webpanobject.remove()
         if(webpanobject == null){
          let browserManagerSidebarWebpanel = document.createXULElement("browser");
          browserManagerSidebarWebpanel.setAttribute("usercontextid", wibpanel_usercontext);
          browserManagerSidebarWebpanel.setAttribute("xmlns", "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");
          browserManagerSidebarWebpanel.setAttribute("id", `webpanel${webpanel_id}`);
          browserManagerSidebarWebpanel.setAttribute("class", "webpanels");
          browserManagerSidebarWebpanel.setAttribute("flex", "1");
          browserManagerSidebarWebpanel.setAttribute("autoscroll", "false");
          browserManagerSidebarWebpanel.setAttribute("disablehistory", "true");
          browserManagerSidebarWebpanel.setAttribute("disablefullscreen", "true");
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
          webpanobject.setAttribute("src", webpanelURL);
          if(webpanobject.getAttribute("changeuseragent") != String(webpanel_userAgent) || webpanobject.getAttribute("usercontextid") != wibpanel_usercontext){
            webpanobject.setAttribute("usercontextid", wibpanel_usercontext);
            webpanobject.setAttribute("changeuseragent", String(webpanel_userAgent));
            webpanobject.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE)
          }

        }
    }
}

function changeSidebarVisibility() {
  let sidebar2 = document.getElementById("sidebar2");
  let siderbar2header = document.getElementById("sidebar2-header");
  let sidebarsplit2 = document.getElementById("sidebar-splitter2");
  let sidebar2box = document.getElementById("sidebar2-box");
  let sidebar2BoolStatus = "floorp.browser.sidebar.is.displayed";

  if (sidebarsplit2.getAttribute("hidden") == "true" || sidebarsplit2.getAttribute("hidden") == "true") {
    sidebar2.style.minWidth = "auto";
    sidebar2.style.maxWidth = "";
    sidebar2box.style.minWidth = "auto";
    sidebar2box.style.maxWidth = "";
    siderbar2header.style.display = "";
    sidebarsplit2.setAttribute("hidden", "false");
    displayIcons();
    setSelectedPanel();
    Services.prefs.setBoolPref(sidebar2BoolStatus, true);
  } else {
    sidebar2.style.minWidth = "0";
    sidebar2.style.maxWidth = "0";
    sidebar2box.style.minWidth = "0";
    sidebar2box.style.maxWidth = "0";
    siderbar2header.style.display = "none";
    sidebarsplit2.setAttribute("hidden", "true");
    hideIcons();
    removeAttributeSelectedNode();
    Services.prefs.setBoolPref(sidebar2BoolStatus, false);
  }
}

function displayBrowserManagerSidebar() {
  if (document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
    changeSidebarVisibility();
  }
}

function setCustomURLFavicon(sbar_id) {
  let sbar_url = BROWSER_SIDEBAR_DATA.data[sbar_id.slice(7)].url;

  try {
    new URL(sbar_url);
  } catch (e) {
    console.error(e);
    document.getElementById(`${sbar_id}`).style.listStyleImage = `url(chrome://devtools/skin/images/globe.svg)`;
    setUserContextLine(sbar_id.slice(7));
    return;
  }

  if(sbar_url.startsWith("http://") || sbar_url.startsWith("https://")) {
    document.getElementById(`${sbar_id}`).style.listStyleImage = `url(chrome://devtools/skin/images/globe.svg)`;

    let iconProvider = Services.prefs.getStringPref("floorp.browser.sidebar.useIconProvider", null);
    let icon_url;
    switch (iconProvider) {
      case "google":
        icon_url = `https://www.google.com/s2/favicons?domain_url=${encodeURIComponent(sbar_url)}`;
        break;
      case "duckduckgo":
        icon_url = `https://external-content.duckduckgo.com/ip3/${(new URL(sbar_url)).hostname}.ico`;
        break;
      case "yandex":
        icon_url = `https://favicon.yandex.net/favicon/v2/${(new URL(sbar_url)).origin}`;
        break;
      case "hatena":
        icon_url = `https://cdn-ak.favicon.st-hatena.com/?url=${encodeURIComponent(sbar_url)}`; // or `https://favicon.hatena.ne.jp/?url=${encodeURIComponent(sbar_url)}`
        break;
      default:
        icon_url = `https://www.google.com/s2/favicons?domain_url=${encodeURIComponent(sbar_url)}`;
        break;
    }

    fetch(icon_url)
      .then(async(response) => {
        if (response.status !== 200) {
          throw `${response.status} ${response.statusText}`;
        }

        let reader = new FileReader();

        let blob_data = await response.blob();

        let icon_data_url = await new Promise(resolve => {
          reader.addEventListener("load", function () {
            resolve(this.result);
          })
          reader.readAsDataURL(blob_data);
        });

        if (icon_data_url === "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4AWIAAYAAAwAABQABggWTzwAAAABJRU5ErkJggg==") {
          // Yandex will return a 1px size icon with status code 200 if the icon is not available. If it matches a specific Data URL, it will be considered as a failure to acquire, and this process will be aborted.
          throw "Yandex 404"
        }

        if (BROWSER_SIDEBAR_DATA.data[sbar_id.slice(7)].url === sbar_url) {  // Check that the URL has not changed after the icon is retrieved.
          document.getElementById(`${sbar_id}`).style.listStyleImage = `url(${icon_data_url})`;
        }
      })
      .catch(reject => {
        console.error(reject);
      });
  } else if (sbar_url.startsWith("moz-extension://")) {
    document.getElementById(`${sbar_id}`).style.listStyleImage = `url(chrome://mozapps/skin/extensions/extensionGeneric.svg)`;

    let addon_id = (new URL(sbar_url)).hostname;
    let addon_base_url = `moz-extension://${addon_id}`
    fetch(addon_base_url + "/manifest.json")
      .then(async(response) => {
        if (response.status !== 200) {
          throw `${response.status} ${response.statusText}`;
        }

        let addon_manifest = await response.json();

        let addon_icon_path = addon_manifest["icons"][
          Math.max(...Object.keys(addon_manifest["icons"]))
        ];
        if (addon_icon_path === undefined) throw "Icon not found.";

        let addon_icon_url = addon_icon_path.startsWith("/") ?
          `${addon_base_url}${addon_icon_path}` :
          `${addon_base_url}/${addon_icon_path}`;

        if (BROWSER_SIDEBAR_DATA.data[sbar_id.slice(7)].url === sbar_url) {  // Check that the URL has not changed after the icon is retrieved.
          document.getElementById(`${sbar_id}`).style.listStyleImage = `url(${addon_icon_url})`;
        }
      })
      .catch(reject => {
        console.error(reject);
      });
  }

  setUserContextLine(sbar_id.slice(7));
}

function setUserContextLine(id){
    const wibpanel_usercontext = BROWSER_SIDEBAR_DATA.data[id].usercontext ?? 0
    const container_list = ContextualIdentityService.getPublicIdentities()
    if(wibpanel_usercontext != 0 && container_list.findIndex(e => e.userContextId === wibpanel_usercontext) != -1){
      let  container_color = container_list[container_list.findIndex(e => e.userContextId === wibpanel_usercontext)].color
      document.getElementById(`select-${id}`).style.borderLeft = `solid 2px ${container_color == "toolbar" ? "var(--toolbar-field-color)" : container_color}`;
    }else{
if(document.getElementById(`select-${id}`).style.border != '1px solid blue')       document.getElementById(`select-${id}`).style.borderLeft = "";
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
function sidebarMouseOver() {
Services.obs.notifyObservers({eventType:"mouseOver",id:event.target.id},"obs-panel")
}
function sidebarMouseOut() {
Services.obs.notifyObservers({eventType:"mouseOut",id:event.target.id},"obs-panel")
}

function sidebarDragStart() {
		event.dataTransfer.setData('text/plain', event.target.id);
	};
function sidebarDragOver() {
		event.preventDefault();
		this.style.borderTop = '2px solid blue';
	};
function sidebarDragLeave() {
		this.style.borderTop = '';
	};
function sidebarDrop() {
		event.preventDefault();
		let id = event.dataTransfer.getData('text/plain');
		let elm_drag = document.getElementById(id);
		this.parentNode.insertBefore(elm_drag, this);
		this.style.borderTop = '';
    BROWSER_SIDEBAR_DATA.index.splice(0);
   for(let elem of document.querySelectorAll(".sicon-list")){BROWSER_SIDEBAR_DATA.index.push(elem.id.replace("select-",""))};
     Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA) );
	};

function setSidebarIconView() {
  for(let elem of BROWSER_SIDEBAR_DATA.index){
    if(document.getElementById(`select-${elem}`) == null){
      let sidebarItem = document.createXULElement("toolbarbutton");
      sidebarItem.id = `select-${elem}`;
      sidebarItem.classList.add("sidepanel-icon");
      sidebarItem.classList.add("sicon-list");
      sidebarItem.setAttribute("oncommand","setSidebarPage()");
     if(BROWSER_SIDEBAR_DATA.data[elem]["url"].slice(0,8) == "floorp//"){
        if(BROWSER_SIDEBAR_DATA.data[elem]["url"] in STATIC_SIDEBAR_L10N_LIST){
          //0~4 - StaticModeSetter | Browser Manager, Bookmark, History, Downloads, TreeStyleTab have l10n & Delete panel
          sidebarItem.setAttribute("data-l10n-id",STATIC_SIDEBAR_L10N_LIST[BROWSER_SIDEBAR_DATA.data[elem].url] );
          sidebarItem.setAttribute("context", "all-panel-context");
        }
     }else{
        //5~ CustomURLSetter | Custom URL have l10n, Userangent, Delete panel & etc...
        sidebarItem.classList.add("webpanel-icon");
        sidebarItem.setAttribute("context","webpanel-context");
     }

  sidebarItem.onmouseover = sidebarMouseOver
  sidebarItem.onmouseout = sidebarMouseOut
  sidebarItem.ondragstart = sidebarDragStart
	sidebarItem.ondragover = sidebarDragOver
	sidebarItem.ondragleave = sidebarDragLeave
	sidebarItem.ondrop = sidebarDrop

  let sidebarItemImage = document.createXULElement("image");
  sidebarItemImage.classList.add("toolbarbutton-icon")
  sidebarItem.appendChild(sidebarItemImage)

  let sidebarItemLabel = document.createXULElement("label");
  sidebarItemLabel.classList.add("toolbarbutton-text")
  sidebarItemLabel.setAttribute("crop","right")
  sidebarItemLabel.setAttribute("flex","1")
  sidebarItem.appendChild(sidebarItemLabel)

      document.getElementById("sidebar-select-box").insertBefore(sidebarItem,document.getElementById("add-button"));
    }else{
      document.getElementById("sidebar-select-box").insertBefore(document.getElementById(`select-${elem}`),document.getElementById("add-button"));
    }
  }
  let siconAll = document.querySelectorAll(".sicon-list")
  let sicon = siconAll.length
  let side = BROWSER_SIDEBAR_DATA.index.length
  if(sicon > side){
    for(let i = 0;i < (sicon - side);i++){
      if(document.getElementById(siconAll[i].id.replace("select-","webpanel")) != null){
        let sidebarsplit2 = document.getElementById("sidebar-splitter2");

        if(Services.prefs.getStringPref("floorp.browser.sidebar2.page", "") == siconAll[i].id.replace("select-","")){
          Services.prefs.setStringPref("floorp.browser.sidebar2.page", "");
          if (sidebarsplit2.getAttribute("hidden") != "true") {
            changeSidebarVisibility();
          }
        }
        document.getElementById(siconAll[i].id.replace("select-","webpanel")).remove();
      }
	      
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

function deleteWebpanel(){
  let sidebarsplit2 = document.getElementById("sidebar-splitter2");

  if (sidebarsplit2.getAttribute("hidden") != "true") {
    changeSidebarVisibility();
  }

  let index = BROWSER_SIDEBAR_DATA.index.indexOf(clickedWebpanel.replace("select-", ""));
  BROWSER_SIDEBAR_DATA.index.splice(index, 1);
  delete BROWSER_SIDEBAR_DATA.data[clickedWebpanel.replace("select-", "")];
  Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));

  //Delete webpanel from sidebar. If it is Browser Manager or TST, Cannot delete.
  try{contextWebpanel.remove();
      document.getElementById(clickedWebpanel).remove();
    } catch(e){};
}

 function muteSidebar(){
    if(contextWebpanel.audioMuted == false){
      contextWebpanel.mute();
    } else {
     contextWebpanel.unmute();
   }  
  }

function AddBMSWebpanel(url,uc){
  let parentWindow = window
  let updateNumberDate = new Date()
  let updateNumber = `w${updateNumberDate.getFullYear()}${updateNumberDate.getMonth()}${updateNumberDate.getDate()}${updateNumberDate.getHours()}${updateNumberDate.getMinutes()}${updateNumberDate.getSeconds()}`
let object = {"new":true,"id":updateNumber}
if(url != "")object.url = url
if(uc != "")object.userContext = uc
  if (
    parentWindow?.document.documentURI ==
    "chrome://browser/content/hiddenWindowMac.xhtml"
  ) {
    parentWindow = null;
  }
  if (parentWindow?.gDialogBox) {
    parentWindow.gDialogBox.open("chrome://browser/content/preferences/dialogs/customURLs.xhtml", object);
  } else {
    Services.ww.openWindow(
      parentWindow,
      "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
      "AddWebpanel",
      "chrome,titlebar,dialog,centerscreen,modal",
      object
    );
  }
}

function obsPanelRe(data_){
    let data = data_.wrappedJSObject
    switch(data.eventType){
      case "mouseOver":
	document.getElementById(data.id.replace("BSB-","select-")).style.border = '1px solid blue';
setUserContextLine(data.id.replace("BSB-",""))
        break
      case "mouseOut":
document.getElementById(data.id.replace("BSB-","select-")).style.border = '';
setUserContextLine(data.id.replace("BSB-",""))
        break
    }
  }

/*---------------------------------------------------------------- design ----------------------------------------------------------------*/

function setBrowserDesign() {
  let floorpinterfacenum = Services.prefs.getIntPref("floorp.browser.user.interface")
  let updateNumberDate = new Date()
  let updateNumber = `${updateNumberDate.getFullYear()}${updateNumberDate.getMonth()}${updateNumberDate.getDate()}${updateNumberDate.getHours()}${updateNumberDate.getMinutes()}${updateNumberDate.getSeconds()}${Math.floor(Math.random()*1000)}`
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

/*---------------------------------------------------------------- Context Menu ----------------------------------------------------------------*/
function addContextBox(id,l10n,insert,runFunction){
  let contextMenu = document.createXULElement("menuitem");
  contextMenu.setAttribute("data-l10n-id",l10n);
  contextMenu.id = id;
  contextMenu.setAttribute("oncommand",runFunction);
  document.getElementById("contentAreaContextMenu").insertBefore(contextMenu,document.getElementById(insert));
  contextMenuObserverFunc();
}

function contextMenuObserverFunc(){
  if(document.getElementById("bsb-context-add") != null) document.getElementById("bsb-context-add").hidden = document.getElementById("context-viewsource").hidden || !document.getElementById("context-viewimage").hidden
  if(document.getElementById("bsb-context-link-add") != null) document.getElementById("bsb-context-link-add").hidden = document.getElementById("context-openlink").hidden
  if(document.getElementById("bsb-context-link-add") != null) document.getElementById("bsb-context-link-add").hidden = document.getElementById("context-openlink").hidden
}

function contextMenuObserverAdd(id){
  contextMenuObserver.observe(document.getElementById(id), {attributes:true})
  contextMenuObserverFunc()
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
      window.setTimeout(function () { document.getElementById("titlebar").before(document.getElementById("toolbar-menubar")); }, 500);
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
/*---------------------------------------------------------------- Multirow-Tab ----------------------------------------------------------------*/

function setMultirowTabMaxHeight() {
  try{document.querySelector("#tabbrowser-arrowscrollbox").shadowRoot.querySelector("[part=scrollbox]").removeAttribute("style")}catch(e){};

  let rowValue = Services.prefs.getIntPref("floorp.browser.tabbar.multirow.max.row");
  let tabHeight = document.querySelector(".tabbrowser-tab").clientHeight;
  document.querySelector("#tabbrowser-arrowscrollbox").shadowRoot.querySelector("[part=scrollbox]").
    setAttribute("style", "max-height: " + tabHeight*rowValue + "px !important;");
}

function removeMultirowTabMaxHeight() {
  document.querySelector("#tabbrowser-arrowscrollbox").shadowRoot.querySelector("[part=scrollbox]").
    removeAttribute("style");
}
