/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
let { BrowserManagerSidebar } = ChromeUtils.importESModule("resource:///modules/BrowserManagerSidebar.sys.mjs")
XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization([
    "branding/brand.ftl",
    "browser/floorp",
  ]);
});

Preferences.addAll([
  { id: "floorp.browser.sidebar.right", type: "bool" },
  { id: "floorp.browser.sidebar.enable", type: "bool" },
  { id: "floorp.browser.sidebar2.mode", type: "int" },
  { id: "floorp.browser.restore.sidebar.panel", type: "bool" },
  { id: "floorp.browser.sidebar.useIconProvider", type: "string" },
  { id: "floorp.browser.sidebar2.hide.to.unload.panel.enabled", type: "bool" },
])

var gBSBPane = {
  _pane: null,
  obsPanel(data_) {
    let data = data_.wrappedJSObject
    switch (data.eventType) {
      case "mouseOver":
        document.getElementById(data.id.replace("select-", "BSB-")).style.border = '1px solid blue';
        break
      case "mouseOut":
        document.getElementById(data.id.replace("select-", "BSB-")).style.border = '';
        break
    }
  },
  mouseOver(id) {
    Services.obs.notifyObservers({ eventType: "mouseOver", id }, "obs-panel-re")
  },
  mouseOut(id) {
    Services.obs.notifyObservers({ eventType: "mouseOut", id }, "obs-panel-re")
  },

  deleteWebpanel(id) { 
    this.BSBs.index = this.BSBs.index.filter(n => n != id);
    delete this.BSBs.data[id];
    Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(this.BSBs))
  },

  upWebpanel(id){
    let index = this.BSBs.index.indexOf(id)
    if(index >= 1){
      let tempValue = this.BSBs.index[index]
      this.BSBs.index[index] = this.BSBs.index[index - 1]
      this.BSBs.index[index -1] = tempValue
      Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(this.BSBs))
      Services.obs.notifyObservers({ eventType: "mouseOut", id: `BSB-${id}` }, "obs-panel-re")
    }
  },

  downWebpanel(id){
    let index = this.BSBs.index.indexOf(id)
    if(index != this.BSBs.index.length - 1 && index != -1){
      let tempValue = this.BSBs.index[index]
      this.BSBs.index[index] = this.BSBs.index[index + 1]
      this.BSBs.index[index +1] = tempValue
      Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(this.BSBs))
      Services.obs.notifyObservers({ eventType: "mouseOut", id: `BSB-${id}` }, "obs-panel-re")
    }
  },

  // called when the document is first parsed
  init() {
    Services.obs.addObserver(this.obsPanel, "obs-panel")
    Services.prefs.addObserver(`floorp.browser.sidebar2.data`, function () {
      this.BSBs = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
      this.panelSet()
    }.bind(this))
    this._list = document.getElementById("BSBView");
    this._pane = document.getElementById("paneBSB");

    {
      let prefName = "floorp.browser.sidebar2.global.webpanel.width";
      let elem = document.getElementById("GlobalWidth");
      elem.value = Services.prefs.getIntPref(prefName, undefined);
      elem.addEventListener('change', function () {
        Services.prefs.setIntPref(prefName, Number(elem.value));
      });
      Services.prefs.addObserver(prefName, function () {
        elem.value = Services.prefs.getIntPref(prefName, undefined);
      });
    }

    this.panelSet()
    document.getElementById("BSBAdd").addEventListener("command", function () {
      let updateNumberDate = new Date()
      let updateNumber = `w${updateNumberDate.getFullYear()}${updateNumberDate.getMonth()}${updateNumberDate.getDate()}${updateNumberDate.getHours()}${updateNumberDate.getMinutes()}${updateNumberDate.getSeconds()}`
      gSubDialog.open(
        "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
        undefined,
        { id: updateNumber, new: true }
      );
    });
    document.getElementById("BSBDefault").addEventListener("command", function () { Services.prefs.clearUserPref(`floorp.browser.sidebar2.data`); });
  },

  setURL(elemUrl, elem) {
    if(elemUrl.startsWith("floorp//")){
      elem.setAttribute("data-l10n-id", "sidebar2-" +  BrowserManagerSidebar.STATIC_SIDEBAR_DATA[elemUrl].l10n);
    }
    else if(elemUrl.startsWith("extension")){
      elem.removeAttribute("data-l10n-id");
      elem.textContent = elemUrl.split(",")[1]
    }
    else{
      elem.removeAttribute("data-l10n-id");
      elem.textContent = elemUrl
    }

  },

  panelSet() {
    this.BSBs = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
    let isFirst = true
    let lastElem = null
    for (let container of this.BSBs.index) {
      let listItem = null
      if (document.getElementById(`BSB-${container}`) == null) {
        listItem = window.MozXULElement.parseXULToFragment(`
        <richlistitem id="BSB-${container}" class="BSB-list">
          <hbox flex="1" align="center">
            <hbox class="userContext-icon userContext-icon-inprefs" width="24" height="24"></hbox>
            <label flex="1" class="bsb_label"></label>
          </hbox>
          <hbox class="container-buttons">
            <button class="BMS-Up">
              <hbox class="box-inherit button-box" align="center" pack="center" flex="1" anonid="button-box">
                <image class="button-icon"/>
                <label class="button-text" value="↑"/>
              </hbox>
            </button>
            <button class="BMS-Down">
              <hbox class="box-inherit button-box" align="center" pack="center" flex="1" anonid="button-box">
                <image class="button-icon"/>
                <label class="button-text" value="↓"/>
              </hbox>
            </button>
            <button class="BMS-Edit"></button>
            <button class="BMS-Remove"></button>
          </hbox>
        </richlistitem>
        `).querySelector("*")
        {
          listItem.onmouseover = function () { this.mouseOver(`BSB-${container}`) }.bind(this)
          listItem.onmouseout = function () { this.mouseOut(`BSB-${container}`) }.bind(this)
        }{
          let elem = listItem.querySelector(".bsb_label")
          this.setURL(this.BSBs.data[container].url, elem)
        }{
          let elem = listItem.querySelector(".BMS-Edit")
          elem.addEventListener("command", function (event) {
            gSubDialog.open(
              "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
              undefined,
              { id: event.target.getAttribute("value"), new: false }
            );
          });
          elem.setAttribute("value", container);
          document.l10n.setAttributes(elem, "sidebar2-pref-setting");
        }{
          let elem = listItem.querySelector(".BMS-Remove")
          elem.addEventListener("command", function (event) {
            this.deleteWebpanel(event.target.getAttribute("value"))
          }.bind(this));
          elem.setAttribute("value", container);
          document.l10n.setAttributes(elem, "sidebar2-pref-delete");
        }{
          let elem = listItem.querySelector(".BMS-Up")
          elem.addEventListener("command", function (event) {
            this.upWebpanel(event.target.getAttribute("value"))
          }.bind(this));
          elem.setAttribute("value", container);
        }
        {
          let elem = listItem.querySelector(".BMS-Down")
          elem.addEventListener("command", function (event) {
            this.downWebpanel(event.target.getAttribute("value"))
          }.bind(this));
          elem.setAttribute("value", container);
        }
        this._list.insertBefore(listItem, document.getElementById("BSBSpace"));
      } else { 
        listItem = document.getElementById(`BSB-${container}`)
        this._list.insertBefore(listItem, document.getElementById("BSBSpace")); 
        this.setURL(this.BSBs.data[container].url, listItem.querySelector(".bsb_label")) 
      }
      listItem?.querySelector(".BMS-Up")?.setAttribute?.("disabled",isFirst ? "true" : "false")
      listItem?.querySelector(".BMS-Down")?.setAttribute?.("disabled","false")
      isFirst = false
      lastElem = listItem
    }
    lastElem?.querySelector(".BMS-Down")?.setAttribute?.("disabled","true")
    let BSBAll = document.querySelectorAll(".BSB-list")
    let sicon = BSBAll.length
    let side = this.BSBs.index.length
    if (sicon > side) {
      for (let i = 0; i < (sicon - side); i++) {
        BSBAll[i].remove()
      }
    }
  }
};


