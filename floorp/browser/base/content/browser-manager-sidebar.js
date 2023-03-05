/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


let { BrowserManagerSidebar } = ChromeUtils.import("resource:///modules/BrowserManagerSidebar.jsm")

/*---------------------------------------------------------------- browser manager sidebar ----------------------------------------------------------------*/

const STATIC_SIDEBAR_DATA = BrowserManagerSidebar.STATIC_SIDEBAR_DATA
let BROWSER_SIDEBAR_DATA = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
const sidebar_icons = ["sidebar2-back", "sidebar2-forward", "sidebar2-reload"]

const bmsController = {
    eventFunctions:{
        sidebarButtons:(action) => {
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
        },
        keepWidth:()=>{
            const pref = Services.prefs.getStringPref("floorp.browser.sidebar2.page");
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

            if (Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined) != custom_url_id || document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
                Services.prefs.setStringPref("floorp.browser.sidebar2.page", custom_url_id);
                if (document.getElementById("sidebar-splitter2").getAttribute("hidden") == "true") {
                   bmsController.controllFunctions.changeVisibleWenpanel();
                }
                bmsController.controllFunctions.changeCheckPanel(true);
            } else {
               bmsController.controllFunctions.changeVisibleWenpanel();
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
                Services.prefs.setStringPref("floorp.browser.sidebar2.page", "");
            
                if (sidebarsplit2.getAttribute("hidden") != "true") {
                   bmsController.controllFunctions.changeVisibleWenpanel();
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
                contextWebpanel.remove();

                if(document.getElementById(clickedWebpanel) != null) document.getElementById(clickedWebpanel).remove();
            },
            muteWebpanel:()=>{
                if (contextWebpanel.audioMuted == false) {
                    contextWebpanel.mute();
                } else {
                    contextWebpanel.unmute();
                }
            }
        }
    },
    controllFunctions:{
        visiblePanelBrowserElem:()=>{
            const modeValuePref = Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined);
            const selectedwebpanel = document.getElementById(`webpanel${modeValuePref}`);
            const selectedURL = BROWSER_SIDEBAR_DATA.data[modeValuePref].url ?? ""
        
            let bmsStyle = document.getElementById("sidebar2style")
            if(bmsStyle == null){
                bmsStyle = document.createElement("style")
                bmsStyle.id = "sidebar2style"
                document.querySelector("head").insertAdjacentElement("beforeend", bmsStyle);
            }
            if (selectedURL == "floorp//tst") {
                bmsStyle.innerText = "#sidebar2{max-height:0 !important;}#sidebar2-reload,#sidebar2-go-index, #sidebar2-forward,#sidebar2-back{display:none !important;}"
            } else if (selectedURL.startsWith("floorp//")) {
                bmsStyle.innerText = "#TST{max-height:0 !important;}#sidebar2-reload, #sidebar2-go-index, #sidebar2-forward,#sidebar2-back{display:none !important;}"
            } else {
                bmsStyle.innerText = "#TST, #sidebar2{max-height:0 !important;}"
            }
        
            for (let elem of document.getElementsByClassName("webpanels")) {
                elem.hidden = true;
            }
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
                let selectedNode = document.querySelector(`#select-${Services.prefs.getStringPref("floorp.browser.sidebar2.page", undefined)}`);
                if(selectedNode != null) selectedNode.setAttribute("checked", "true")
            }
        },
        changeVisibleWenpanel:()=>{
            let sidebar2 = document.getElementById("sidebar2");
            let siderbar2header = document.getElementById("sidebar2-header");
            let sidebarsplit2 = document.getElementById("sidebar-splitter2");
            let sidebar2box = document.getElementById("sidebar2-box");
            let sidebarSetting = {true:["auto","","auto","","","false",false,true,true],false:["0","0","0","0","none","true",true,false,false]}
            let doDisplay = sidebarsplit2.getAttribute("hidden") == "true"
        
            sidebar2.style.minWidth = sidebarSetting[doDisplay][0];
            sidebar2.style.maxWidth = sidebarSetting[doDisplay][1];
            sidebar2box.style.minWidth = sidebarSetting[doDisplay][2];
            sidebar2box.style.maxWidth = sidebarSetting[doDisplay][3];
            siderbar2header.style.display = sidebarSetting[doDisplay][4];
            sidebarsplit2.setAttribute("hidden", sidebarSetting[doDisplay][5]);
            for (let elem of sidebar_icons) document.getElementById(elem).hidden = sidebarSetting[doDisplay][6];
            bmsController.controllFunctions.changeCheckPanel(sidebarSetting[doDisplay][7]);
            Services.prefs.setBoolPref("floorp.browser.sidebar.is.displayed", sidebarSetting[doDisplay][8]);
        },
        setSidebarWidth:(webpanel_id)=>{
            if (webpanel_id != "" && BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)) {
                const panelWidth = BROWSER_SIDEBAR_DATA.data[webpanel_id].width ?? Services.prefs.getIntPref("floorp.browser.sidebar2.global.webpanel.width", undefined);
                document.getElementById("sidebar2-box").setAttribute("width", panelWidth);
            }
        },
        visibleWebpanel:()=>{
            const webpanel_id = Services.prefs.getStringPref("floorp.browser.sidebar2.page", "");
            if (webpanel_id != "" && BROWSER_SIDEBAR_DATA.index.includes(webpanel_id)) {
                bmsController.controllFunctions.makeWebpanel(webpanel_id)
            }
        },
        makeWebpanel:(webpanel_id)=>{
            const webpandata = BROWSER_SIDEBAR_DATA.data[webpanel_id]
            let webpanobject = document.getElementById(`webpanel${webpanel_id}`)
            const webpanelURL = webpandata.url
            const sidebar2elem = document.getElementById("sidebar2");
            const wibpanel_usercontext = webpandata.usercontext ?? 0
            const webpanel_userAgent = webpandata.userAgent ?? false
        
            bmsController.controllFunctions.setSidebarWidth(webpanel_id)
            if (webpanelURL.startsWith("floorp//")) {
                let setURL = STATIC_SIDEBAR_DATA[webpanelURL].url
                if (setURL != "none") sidebar2elem.setAttribute("src", setURL);
            } else {
                if (webpanobject != null && (webpanobject.getAttribute("usercontextid") != wibpanel_usercontext)){
                    webpanobject.remove()
                    webpanobject = null
                } 
                if (webpanobject == null) {
                    document.getElementById("sidebar2-box").appendChild(window.MozXULElement.parseXULToFragment(`
                  <browser usercontextid="${wibpanel_usercontext}" xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
                    id="webpanel${webpanel_id}" class="webpanels" flex="1" autoscroll="false" disablehistory="true" 
                    disablefullscreen="true" tooltip="aHTMLTooltip" disableglobalhistory="true" messagemanagergroup="webext-browsers"
                    context="" webextension-view-type="sidebar" autocompletepopup="PopupAutoComplete"
                    initialBrowsingContextGroupId="40" type="content" remote="true" changeuseragent="${String(webpanel_userAgent)}"
                    maychangeremoteness="true" src="${webpanelURL}"/>
                  `));
                } else {
                    webpanobject.setAttribute("src", webpanelURL);
                    if (webpanobject.getAttribute("changeuseragent") !== String(webpanel_userAgent) || webpanobject.getAttribute("usercontextid") !== String(wibpanel_usercontext)) {
                        webpanobject.setAttribute("usercontextid", wibpanel_usercontext);
                        webpanobject.setAttribute("changeuseragent", String(webpanel_userAgent));
                        webpanobject.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE)
                    }
                }
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
        
                        if (Services.prefs.getStringPref("floorp.browser.sidebar2.page", "") == siconAll[i].id.replace("select-", "")) {
                            Services.prefs.setStringPref("floorp.browser.sidebar2.page", "");
                            if (sidebarsplit2.getAttribute("hidden") != "true") {
                               bmsController.controllFunctions.changeVisibleWenpanel();
                            }
                        }
                        document.getElementById(siconAll[i].id.replace("select-", "webpanel")).remove();
                    }
                    siconAll[i].remove()
                }
            }
        
            for (let elem of document.querySelectorAll(".webpanel-icon")) {
                let sbar_url = BROWSER_SIDEBAR_DATA.data[elem.id.slice(7)].url;
                BrowserManagerSidebar.getFavicon(sbar_url, document.getElementById(`${elem.id}`))
                bmsController.controllFunctions.setUserContextColorLine(elem.id.slice(7));
            }
        }
    }
}

if(Services.prefs.getBoolPref("floorp.browser.sidebar.enable", false)){
    (() => {
        Services.prefs.setStringPref("floorp.browser.sidebar2.page","")
        // Context Menu
        addContextBox("bsb-context-add", "bsb-context-add", "fill-login", `
        BrowserManagerSidebar.addPanel(gContextMenu.browser.currentURI.spec,gContextMenu.browser.getAttribute("usercontextid") ?? 0)
        `,"context-viewsource",function(){document.getElementById("bsb-context-add").hidden = document.getElementById("context-viewsource").hidden || !document.getElementById("context-viewimage").hidden})
        
        addContextBox("bsb-context-link-add", "bsb-context-link-add", "context-sep-sendlinktodevice", `
        BrowserManagerSidebar.addPanel(gContextMenu.linkURL,gContextMenu.browser.getAttribute("usercontextid") ?? 0)
        `,"context-openlink",function(){document.getElementById("bsb-context-link-add").hidden = document.getElementById("context-openlink").hidden})

        Services.prefs.addObserver("floorp.browser.sidebar2.page",bmsController.controllFunctions.visibleWebpanel)
        Services.prefs.addObserver("floorp.browser.sidebar2.global.webpanel.width",  () => bmsController.controllFunctions.setSidebarWidth(Services.prefs.getStringPref("floorp.browser.sidebar2.page", "")))

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

        // Set TST URL
        window.setTimeout(async () => {
            document.getElementById("TST").setAttribute("src", await BrowserManagerSidebar.getAdoonSidebarPage("treestyletab@piro.sakura.ne.jp"));
        }, 1250);
    })()
}
