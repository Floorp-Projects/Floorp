/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


 let { BrowserManagerSidebar } = ChromeUtils.import("resource:///modules/BrowserManagerSidebar.jsm")
 var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm")

 /*---------------------------------------------------------------- browser manager sidebar ----------------------------------------------------------------*/
 
 const STATIC_SIDEBAR_DATA = BrowserManagerSidebar.STATIC_SIDEBAR_DATA
 let BROWSER_SIDEBAR_DATA = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
 const sidebar_icons = ["sidebar2-back", "sidebar2-forward", "sidebar2-reload", "sidebar2-go-index"]
 
 const bmsController = {
     eventFunctions:{
         sidebarButtons:(action) => {
             const modeValuePref = bmsController.nowPage
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
                     webpanel.gotoIndex();
                     break
             }
         },
         keepWidth:()=>{
             const pref = bmsController.nowPage;
             BROWSER_SIDEBAR_DATA.data[pref].width = document.getElementById("sidebar2-box").width
             Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));
         },
         keepWidthForGlobal:()=>{
             Services.prefs.setIntPref("floorp.browser.sidebar2.global.webpanel.width", document.getElementById("sidebar2-box").width);
         },
         servicesObs:(data_) => {
             let data = data_.wrappedJSObject
             switch (data.eventType) {
                 case "mouseOver":
                     document.getElementById(data.id.replace("BSB-", "select-")).style.border = '1px solid blue';
                     bmsController.controllFunctions.setUserContextColorLine(data.id.replace("BSB-", ""))
                     break
                 case "mouseOut":
                     document.getElementById(data.id.replace("BSB-", "select-")).style.border = '';
                     bmsController.controllFunctions.setUserContextColorLine(data.id.replace("BSB-", ""))
                     break
             }
         },
         selectSidebarItem:()=>{
             let custom_url_id = event.target.id.replace("select-", "")

             if(bmsController.nowPage == custom_url_id){
                bmsController.controllFunctions.changeVisibleWenpanel();
             }else{
                bmsController.nowPage = custom_url_id
                bmsController.controllFunctions.visibleWebpanel()
             }
         },
         sidebarItemMouse:{
             mouseOver:(event)=>Services.obs.notifyObservers({ eventType: "mouseOver", id: event.target.id }, "obs-panel"),
             mouseOut:(event)=>Services.obs.notifyObservers({ eventType: "mouseOut", id: event.target.id }, "obs-panel"),
             dragStart:(event) => event.dataTransfer.setData('text/plain', event.target.id),
             dragOver:(event) => {
                 event.preventDefault();
                 event.currentTarget.style.borderTop = '2px solid blue';
             },
             dragLeave:(event) => event.currentTarget.style.borderTop = '',
             drop:(event) => {
                 event.preventDefault();
                 let id = event.dataTransfer.getData('text/plain');
                 let elm_drag = document.getElementById(id);
                 event.currentTarget.parentNode.insertBefore(elm_drag, event.currentTarget);
                 event.currentTarget.style.borderTop = '';
                 BROWSER_SIDEBAR_DATA.index.splice(0);
                 for (let elem of document.querySelectorAll(".sicon-list")) {
                     BROWSER_SIDEBAR_DATA.index.push(elem.id.replace("select-", "")) 
                 };
                 Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));
             }
         },
         contextMenu:{
             show:(event)=>{
                 clickedWebpanel = event.explicitOriginalTarget.id;
                 webpanel = clickedWebpanel.replace("select-", "webpanel");
                 contextWebpanel = document.getElementById(webpanel);
                 needLoadedWebpanel = document.getElementsByClassName("needLoadedWebpanel");
                 for (let i = 0; i < needLoadedWebpanel.length; i++) {
                     needLoadedWebpanel[i].disabled = contextWebpanel == null;
                 }
             },
             unloadWebpanel:()=>{
                 let sidebarsplit2 = document.getElementById("sidebar-splitter2");
                 if(clickedWebpanel.replace("select-", "") == bmsController.nowPage){
                    bmsController.nowPage = null
                    bmsController.controllFunctions.visibleWebpanel()
             
                    if (sidebarsplit2.getAttribute("hidden") != "true") {
                        bmsController.controllFunctions.changeVisibleWenpanel();
                    }
                 }
                 contextWebpanel.remove();
             },
             changeUserAgent:()=>{
                 BROWSER_SIDEBAR_DATA.data[clickedWebpanel.replace("select-", "")].userAgent = ((document.getElementById(clickedWebpanel.replace("select-", "webpanel")).getAttribute("changeuseragent") ?? "false") == "false")
 
                 Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));
             
                 //reload webpanel
                 let webpanel = document.getElementById(clickedWebpanel.replace("select-", "webpanel"))
                 if(webpanel != null) webpanel.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE)
             },
             deleteWebpanel:() =>{
                 if (document.getElementById("sidebar-splitter2").getAttribute("hidden") != "true") {
                    bmsController.controllFunctions.changeVisibleWenpanel();
                 }
             
                 let index = BROWSER_SIDEBAR_DATA.index.indexOf(clickedWebpanel.replace("select-", ""));
                 BROWSER_SIDEBAR_DATA.index.splice(index, 1);
                 delete BROWSER_SIDEBAR_DATA.data[clickedWebpanel.replace("select-", "")];
                 Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(BROWSER_SIDEBAR_DATA));
                 contextWebpanel?.remove();

                 document.getElementById(clickedWebpanel)?.remove();
             },
             muteWebpanel:()=>{
                 if (contextWebpanel.audioMuted) {
                     contextWebpanel.unmute();
                 } else {
                    contextWebpanel.mute();
                 }
             }
         }
     },
     controllFunctions:{
         visiblePanelBrowserElem:()=>{
             const modeValuePref = bmsController.nowPage;
             const selectedwebpanel = document.getElementById(`webpanel${modeValuePref}`);
             const selectedURL = BROWSER_SIDEBAR_DATA.data[modeValuePref].url ?? ""
             bmsController.controllFunctions.changeVisibleCommandButton(selectedURL.startsWith("floorp//"))
             for (let elem of document.getElementsByClassName("webpanels")) {
                 elem.hidden = true;
             }
             if(document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true"){
                bmsController.controllFunctions.changeVisibleWenpanel();
             }
             bmsController.controllFunctions.changeCheckPanel(document.getElementById("sidebar-splitter2").getAttribute("hidden") != "true");
             if (selectedwebpanel != null) selectedwebpanel.hidden = false;
         },
         setUserContextColorLine:(id)=>{
             const wibpanel_usercontext = BROWSER_SIDEBAR_DATA.data[id].usercontext ?? 0
             const container_list = ContextualIdentityService.getPublicIdentities()
             if (wibpanel_usercontext != 0 && container_list.findIndex(e => e.userContextId === wibpanel_usercontext) != -1) {
                 let container_color = container_list[container_list.findIndex(e => e.userContextId === wibpanel_usercontext)].color
                 document.getElementById(`select-${id}`).style.borderLeft = `solid 2px ${container_color == "toolbar" ? "var(--toolbar-field-color)" : container_color}`;
             } else if (document.getElementById(`select-${id}`).style.border != '1px solid blue') {
                 document.getElementById(`select-${id}`).style.borderLeft = "";
             }
         },
         changeCheckPanel:(doChecked)=>{
             for (let elem of document.getElementsByClassName("sidepanel-icon")) {
                 elem.setAttribute("checked", "false");
             }
             if(doChecked){
                 let selectedNode = document.querySelector(`#select-${bmsController.nowPage}`);
                 if(selectedNode != null) selectedNode.setAttribute("checked", "true")
             }
         },
         changeVisibleCommandButton:(hidden)=>{
            for (let elem of sidebar_icons) document.getElementById(elem).hidden = hidden
         },
         changeVisibleWenpanel:()=>{
             let siderbar2header = document.getElementById("sidebar2-header");
             let sidebarsplit2 = document.getElementById("sidebar-splitter2");
             let sidebar2box = document.getElementById("sidebar2-box");
             let sidebarSetting = {true:["auto","","","false"],false:["0","0","none","true"]}
             let doDisplay = sidebarsplit2.getAttribute("hidden") == "true"
         
             sidebar2box.style.minWidth = sidebarSetting[doDisplay][0];
             sidebar2box.style.maxWidth = sidebarSetting[doDisplay][1];
             siderbar2header.style.display = sidebarSetting[doDisplay][2];
             sidebarsplit2.setAttribute("hidden", sidebarSetting[doDisplay][3]);
             bmsController.controllFunctions.changeCheckPanel(doDisplay);
             Services.prefs.setBoolPref("floorp.browser.sidebar.is.displayed", doDisplay);
         },
         setSidebarWidth:(webpanel_id)=>{
             if (webpanel_id != null && BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)) {
                 const panelWidth = BROWSER_SIDEBAR_DATA.data[webpanel_id].width ?? Services.prefs.getIntPref("floorp.browser.sidebar2.global.webpanel.width", undefined);
                 document.getElementById("sidebar2-box").setAttribute("width", panelWidth);
             }
         },
         visibleWebpanel:()=>{
             const webpanel_id = bmsController.nowPage;
             if (webpanel_id != null && BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)) {
                 bmsController.controllFunctions.makeWebpanel(webpanel_id)
             }
         },
         makeWebpanel:(webpanel_id)=>{
             const webpandata = BROWSER_SIDEBAR_DATA.data[webpanel_id]
             let webpanobject = document.getElementById(`webpanel${webpanel_id}`)
             let webpanelURL = webpandata.url
             const wibpanel_usercontext = webpandata.usercontext ?? 0
             const webpanel_userAgent = webpandata.userAgent ?? false
             let isWeb = true
             let isTST = false
             let isFloorp = false
         
             bmsController.controllFunctions.setSidebarWidth(webpanel_id)
             if (webpanelURL.startsWith("floorp//")) {
                if(webpanelURL == "floorp//tst") isTST = true
                isFloorp = true
                 webpanelURL = STATIC_SIDEBAR_DATA[webpanelURL].url
                 isWeb = false
             }
             if(webpanobject != null && (
                (
                  !(webpanobject?.getAttribute("changeuseragent") == "" && !webpanel_userAgent) && 
                  !(webpanobject?.getAttribute("usercontextid") == "" && wibpanel_usercontext == 0) && 
                  ((webpanobject?.getAttribute("changeuseragent") ?? "false") !== String(webpanel_userAgent) || (webpanobject?.getAttribute("usercontextid") ?? "0") !== String(wibpanel_usercontext))
                )
                ||
                (
                    ((webpanobject.className.includes("isFloorp") || webpanobject.className.includes("isTST")) && isWeb) || 
                    ((webpanobject.className.includes("isFloorp") || webpanobject.className.includes("isWeb")) && isTST) ||
                    ((webpanobject.className.includes("isTST") || webpanobject.className.includes("isWeb")) && isFloorp)
                )
                )              
            ){
                webpanobject.remove()
                webpanobject = null
            } 
            if (webpanobject == null) {
                let webpanelElem = window.MozXULElement.parseXULToFragment(`
              <browser 
                id="webpanel${webpanel_id}"
                class="webpanels${isFloorp ? " isFloorp" : (isTST ? " isTST" : " isWeb")}"
                flex="1"
                xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
                disablehistory="true"
                disablefullscreen="true"
                tooltip="aHTMLTooltip"
                autoscroll="false"
                disableglobalhistory="true"
                messagemanagergroup="browsers"
                autocompletepopup="PopupAutoComplete"
                initialBrowsingContextGroupId="40"
                ${isWeb ? `
                usercontextid="${(typeof wibpanel_usercontext) == "number" ? String(wibpanel_usercontext) : "0"}"
                changeuseragent="${webpanel_userAgent == true}"
                ` :""}
                ${isWeb || isTST ? `
                webextension-view-type="sidebar"
                type="content"
                remote="true"
                maychangeremoteness="true"
                ` :""}
                ${isWeb ? `context=""`
                  : (isTST ? `context="contentAreaContextMenu"` : "")
                }
                 />
              `)
                webpanelElem.firstChild.setAttribute("src", webpanelURL);
                document.getElementById("sidebar2-box").appendChild(webpanelElem);
            } else {
                webpanobject.setAttribute("src", webpanelURL);
            }
             bmsController.controllFunctions.visiblePanelBrowserElem()
         },
         makeSidebarIcon:()=>{
             for (let elem of BROWSER_SIDEBAR_DATA.index) {
                 if (document.getElementById(`select-${elem}`) == null) {
                     let sidebarItem = document.createXULElement("toolbarbutton");
                     sidebarItem.id = `select-${elem}`;
                     sidebarItem.classList.add("sidepanel-icon");
                     sidebarItem.classList.add("sicon-list");
                     sidebarItem.setAttribute("oncommand", "bmsController.eventFunctions.selectSidebarItem()");
                     if (BROWSER_SIDEBAR_DATA.data[elem]["url"].slice(0, 8) == "floorp//") {
                         if (BROWSER_SIDEBAR_DATA.data[elem]["url"] in STATIC_SIDEBAR_DATA) {
                             //0~4 - StaticModeSetter | Browser Manager, Bookmark, History, Downloads, TreeStyleTab have l10n & Delete panel
                             sidebarItem.setAttribute("data-l10n-id","show-" +  STATIC_SIDEBAR_DATA[BROWSER_SIDEBAR_DATA.data[elem].url].l10n);
                             sidebarItem.setAttribute("context", "all-panel-context");
                         }
                     } else {
                         //5~ CustomURLSetter | Custom URL have l10n, Userangent, Delete panel & etc...
                         sidebarItem.classList.add("webpanel-icon");
                         sidebarItem.setAttribute("context", "webpanel-context");
                     }
         
                     sidebarItem.onmouseover = bmsController.eventFunctions.sidebarItemMouse.mouseOver
                     sidebarItem.onmouseout = bmsController.eventFunctions.sidebarItemMouse.mouseOut
                     sidebarItem.ondragstart = bmsController.eventFunctions.sidebarItemMouse.dragStart
                     sidebarItem.ondragover = bmsController.eventFunctions.sidebarItemMouse.dragOver
                     sidebarItem.ondragleave = bmsController.eventFunctions.sidebarItemMouse.dragLeave
                     sidebarItem.ondrop = bmsController.eventFunctions.sidebarItemMouse.drop
         
                     let sidebarItemImage = document.createXULElement("image");
                     sidebarItemImage.classList.add("toolbarbutton-icon")
                     sidebarItem.appendChild(sidebarItemImage)
         
                     let sidebarItemLabel = document.createXULElement("label");
                     sidebarItemLabel.classList.add("toolbarbutton-text")
                     sidebarItemLabel.setAttribute("crop", "right")
                     sidebarItemLabel.setAttribute("flex", "1")
                     sidebarItem.appendChild(sidebarItemLabel)
         
                     document.getElementById("sidebar-select-box").insertBefore(sidebarItem, document.getElementById("add-button"));
                 } else {
                     sidebarItem = document.getElementById(`select-${elem}`)
                     if (BROWSER_SIDEBAR_DATA.data[elem]["url"].slice(0, 8) == "floorp//") {
                         if (BROWSER_SIDEBAR_DATA.data[elem]["url"] in STATIC_SIDEBAR_DATA) {
                             sidebarItem.classList.remove("webpanel-icon");
                             sidebarItem.setAttribute("data-l10n-id","show-" +  STATIC_SIDEBAR_DATA[BROWSER_SIDEBAR_DATA.data[elem].url].l10n);
                             sidebarItem.setAttribute("context", "all-panel-context");
                         }
                     } else {
                         sidebarItem.classList.add("webpanel-icon");
                         sidebarItem.removeAttribute("data-l10n-id");
                         sidebarItem.setAttribute("context", "webpanel-context");
                     }
                     document.getElementById("sidebar-select-box").insertBefore(sidebarItem, document.getElementById("add-button"));
                 }
             }
             let siconAll = document.querySelectorAll(".sicon-list")
             let sicon = siconAll.length
             let side = BROWSER_SIDEBAR_DATA.index.length
             if (sicon > side) {
                 for (let i = 0; i < (sicon - side); i++) {
                     if (document.getElementById(siconAll[i].id.replace("select-", "webpanel")) != null) {
                         let sidebarsplit2 = document.getElementById("sidebar-splitter2");
         
                         if (bmsController.nowPage == siconAll[i].id.replace("select-", "")) {
                            bmsController.nowPage = null
                            bmsController.controllFunctions.visibleWebpanel()
                             if (sidebarsplit2.getAttribute("hidden") != "true") {
                                bmsController.controllFunctions.changeVisibleWenpanel();
                             }
                         }
                         document.getElementById(siconAll[i].id.replace("select-", "webpanel")).remove();
                     }
                     siconAll[i].remove()
                 }
             }
         
             for (let elem of document.querySelectorAll(".sidepanel-icon")) {
                if(elem.className.includes("webpanel-icon")){
                    let sbar_url = BROWSER_SIDEBAR_DATA.data[elem.id.slice(7)].url;
                    BrowserManagerSidebar.getFavicon(sbar_url, document.getElementById(`${elem.id}`))
                    bmsController.controllFunctions.setUserContextColorLine(elem.id.slice(7));
                }else{
                    elem.style.removeProperty("--BMSIcon")
                }

             }
         }
     },
     "nowPage":null
 }
 
 if(Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)){
     (async () => {
         // Context Menu
         addContextBox("bsb-context-add", "bsb-context-add", "fill-login", `
         BrowserManagerSidebar.addPanel(gContextMenu.browser.currentURI.spec,gContextMenu.browser.getAttribute("usercontextid") ?? 0)
         `,"context-viewsource",function(){document.getElementById("bsb-context-add").hidden = document.getElementById("context-viewsource").hidden || !document.getElementById("context-viewimage").hidden})
         
         addContextBox("bsb-context-link-add", "bsb-context-link-add", "context-sep-sendlinktodevice", `
         BrowserManagerSidebar.addPanel(gContextMenu.linkURL,gContextMenu.browser.getAttribute("usercontextid") ?? 0)
         `,"context-openlink",function(){document.getElementById("bsb-context-link-add").hidden = document.getElementById("context-openlink").hidden})
 
         Services.prefs.addObserver("floorp.browser.sidebar2.global.webpanel.width",  () => bmsController.controllFunctions.setSidebarWidth(bmsController.nowPage))
 
         Services.prefs.addObserver(`floorp.browser.sidebar2.data`, function () {
             let TEMP_BROWSER_SIDEBAR_DATA = JSON.parse(JSON.stringify(BROWSER_SIDEBAR_DATA))
             BROWSER_SIDEBAR_DATA = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
             for (let elem of BROWSER_SIDEBAR_DATA.index) {
                if (document.querySelector(`#webpanel${elem}`) && (JSON.stringify(BROWSER_SIDEBAR_DATA.data[elem]) != JSON.stringify(TEMP_BROWSER_SIDEBAR_DATA.data[elem]))) {
                     bmsController.controllFunctions.makeWebpanel(elem)
                 }
             }
             bmsController.controllFunctions.makeSidebarIcon()
             bmsController.controllFunctions.visibleWebpanel()
         })
         Services.obs.addObserver(bmsController.eventFunctions.servicesObs, "obs-panel-re")
         Services.obs.addObserver(bmsController.controllFunctions.changeVisibleWenpanel, "floorp-change-panel-show")
         let addbutton = document.getElementById("add-button")
         addbutton.ondragover = bmsController.eventFunctions.sidebarItemMouse.dragOver
         addbutton.ondragleave = bmsController.eventFunctions.sidebarItemMouse.dragLeave
         addbutton.ondrop = bmsController.eventFunctions.sidebarItemMouse.drop
 
         //startup functions
         bmsController.controllFunctions.makeSidebarIcon();
 
         // sidebar display
         let sidebarsplit2 = document.getElementById("sidebar-splitter2");
         if (!(sidebarsplit2.getAttribute("hidden") == "true")) {
            bmsController.controllFunctions.changeVisibleWenpanel();
         }
         window.bmsController = bmsController
 
         // Set TST URL
         window.setTimeout(async () => {
            BrowserManagerSidebar.STATIC_SIDEBAR_DATA["floorp//tst"].url = await BrowserManagerSidebar.getAdoonSidebarPage("treestyletab@piro.sakura.ne.jp")
         }, 1250);
     })()
 }
