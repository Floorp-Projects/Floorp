/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */
const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);
let { BrowserManagerSidebar } = ChromeUtils.importESModule("resource:///modules/BrowserManagerSidebar.sys.mjs")
function setTitle() {
  let params = window.arguments[0] || {};
  let winElem = document.documentElement;
  if (params.new) {
    document.l10n.setAttributes(winElem, "bsb-add-title");
  } else {
    document.l10n.setAttributes(winElem, "bsb-setting-title");
  }
}

setTitle();
var bsbObject = {}
var panelId = ""
var newPanel = false

function onLoad() {
  bsbObject = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
  let paramsTemp = window.arguments[0] || {};
  let params = {}
  if (window.arguments[0].id ?? "" != "") {
    params = window.arguments[0]
  } else if (window.arguments[0].wrappedJSObject.id ?? "" != "") {
    params = window.arguments[0].wrappedJSObject
  }
  newPanel = params.new
  panelId = params.id
  if (!newPanel) {Services.obs.notifyObservers({ eventType: "mouseOver", id: `BSB-${panelId}` }, "obs-panel-re")}
  let panelUserAgent = newPanel ? false : bsbObject.data[panelId].userAgent ?? false
  let panelWidth = newPanel ? 0 : bsbObject.data[panelId].width ?? 0

  document.addEventListener("dialogaccept", setPref);

  let panelUserContext = params.userContext ?? (newPanel ? -1 : bsbObject.data[panelId].usercontext)

  for(let elem in BrowserManagerSidebar.STATIC_SIDEBAR_DATA){
    let floorpWebpanelMenuitem = document.createXULElement("menuitem")
    floorpWebpanelMenuitem.setAttribute("flex", 1)
    floorpWebpanelMenuitem.setAttribute("data-l10n-id", "bsb-" + BrowserManagerSidebar.STATIC_SIDEBAR_DATA[elem].l10n)
    floorpWebpanelMenuitem.value = elem
    document.querySelector("#pageSelect > menupopup").appendChild(floorpWebpanelMenuitem)
  }

  let url = params.url ?? (newPanel ? "" : (bsbObject.data[params.id].url))
  if(url in BrowserManagerSidebar.STATIC_SIDEBAR_DATA){
    document.querySelector("#pageSelect").value = url
  }
  else if(url.startsWith("extension,")){
    let haveSidebarPanelExtensionsObjPref = "floorp.extensions.webextensions.sidebar-action";
    let extensionObj = JSON.parse(Services.prefs.getStringPref(haveSidebarPanelExtensionsObjPref));
    document.querySelector("#pageSelect").value = url
    document.querySelector("#pageSelect").setAttribute("label", extensionObj.data[url.split(",")[2]].title)
  }else{
    document.querySelector(".URLBox").value = url
  }

  setTimeout(setBox, 500)

  document.querySelector("#userAgentCheck").checked = panelUserAgent;
  document.querySelector("#widthBox").value = panelWidth;

  let container_label = -1
  let container_list = document.querySelector("#userContextPopup")
  let container_list_base = document.querySelector("#userContext")
  let menuitem = document.createXULElement("menuitem");
  container_list_base.value = 0
  menuitem.value = 0
  menuitem.setAttribute("flex", 1);
  let containerName = document.getElementById("browserBundle").getString("userContextNone.label");
  menuitem.setAttribute("label", containerName);
  container_list.appendChild(menuitem);

  for (let elem of ContextualIdentityService.getPublicIdentities()) {
    menuitem = document.createXULElement("menuitem");
    menuitem.value = elem.userContextId
    menuitem.setAttribute("flex", 1);
    containerName = ContextualIdentityService.getUserContextLabel(elem.userContextId);
    if (panelUserContext == elem.userContextId) {
      container_label = ContextualIdentityService.getUserContextLabel(elem.userContextId)
      container_list_base.value = elem.userContextId
    }
    menuitem.setAttribute("label", containerName);
    container_list.appendChild(menuitem);
  }
  if (container_label === -1) {container_label = document.getElementById("browserBundle").getString("userContextNone.label")}
  container_list.parentElement.setAttribute("label", container_label)


  // Sidebar panel extensions
  let { AddonManager } = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
  let haveSidebarPanelExtensionsObjPref = "floorp.extensions.webextensions.sidebar-action";
  let extensionObj = JSON.parse(Services.prefs.getStringPref(haveSidebarPanelExtensionsObjPref));
  let extensionIDs = Object.keys(extensionObj.data);

  for (let extensionID of extensionIDs) {
    AddonManager.getAddonByID(extensionID).then(function (addon) {
      if (addon && addon.isActive) {
        let menuitem = document.createXULElement("menuitem");
        menuitem.value = extensionID
        menuitem.setAttribute("flex", 1);
        menuitem.setAttribute("label", extensionObj.data[addon.id].title);
        menuitem.setAttribute("value", `extension,${extensionObj.data[addon.id].title},${extensionID},${extensionObj.data[addon.id].panel},${extensionObj.data[addon.id].icon}`);
        document.querySelector("#pageSelect > menupopup").appendChild(menuitem)
      }
    });
  }
}

function encodeObjectURL(text) {
  var remove_whitespace = /^\s+/
  var box_value = text
  box_value = box_value.replace(remove_whitespace, ""); / * Removing whitespace from the beginning of a line * /
  if (box_value == "") { /* empty */ }
  else if (!box_value.startsWith("http://") && !box_value.startsWith("https://")) {
    / * Checks if url in the sidebar contains https in the beginning of a line * /
    if (!box_value.startsWith("file://") && !box_value.startsWith("resource://") && !box_value.startsWith("about:") && !box_value.startsWith("jar:") && !box_value.startsWith("about:") && !box_value.startsWith("chrome://") && !box_value.startsWith("moz-extension://")) {
      / * Checks if given URL is other protocol * /
      box_value = `https://${box_value}`;
    }
  }
  return box_value
}

function setPref() {
  let page = Number(document.querySelector("#pageSelect").value)
  let url = document.querySelector(".URLBox").value
  let container = Number(document.querySelector("#userContext").value)
  let userAgent = document.querySelector("#userAgentCheck").checked
  let width = Number(document.querySelector("#widthBox").value)

  let dataObject = {}
  if (page != 0) {
    dataObject.url = document.querySelector("#pageSelect").value
  } else {
    dataObject.url = encodeObjectURL(url)
    if (container != 0) {dataObject.usercontext = container}
    if (userAgent != 0) {dataObject.userAgent = userAgent}
  }
  if (width != 0) {dataObject.width = width}

  bsbObject.data[panelId] = dataObject
  if (newPanel) {bsbObject.index.push(panelId)}

  Services.prefs.setStringPref("floorp.browser.sidebar2.data", JSON.stringify(bsbObject))
}

function setBox() {
  let style = document.querySelector("#pageSelect").value == 0 ? "" : "hidden"
  let elems = document.querySelectorAll(".invisible")
  for (let elem of elems) {
    elem.style.visibility = style
  }
}

function onunload() {
  if (!newPanel) {Services.obs.notifyObservers({ eventType: "mouseOut", id: `BSB-${panelId}` }, "obs-panel-re")}
}
