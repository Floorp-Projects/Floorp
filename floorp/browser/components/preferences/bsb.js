/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
let { BrowserManagerSidebar } = ChromeUtils.import("resource:///modules/BrowserManagerSidebar.jsm")
XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization([
    "branding/brand.ftl",
    "browser/floorp",
  ]);
});

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
    Services.obs.notifyObservers({ eventType: "mouseOver", id: id }, "obs-panel-re")
  },
  mouseOut(id) {
    Services.obs.notifyObservers({ eventType: "mouseOut", id: id }, "obs-panel-re")
  },

  deleteWebpanel(id) { 
    this.BSBs.index = this.BSBs.index.filter(n => n != id);
    delete this.BSBs.data[id];
    Services.prefs.setStringPref(`floorp.browser.sidebar2.data`, JSON.stringify(this.BSBs))
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
    this.panelSet()
    document.getElementById("backtogeneral_").addEventListener("command", function () { gotoPref("general"); });
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
    }else{
      elem.removeAttribute("data-l10n-id");
      elem.textContent = elemUrl
    }
    
  },

  panelSet() {
    this.BSBs = JSON.parse(Services.prefs.getStringPref(`floorp.browser.sidebar2.data`, undefined))
    for (let container of this.BSBs.index) {
      if (document.getElementById(`BSB-${container}`) == null) {
        let item = document.createXULElement("richlistitem");
        item.id = `BSB-${container}`
        item.classList.add("BSB-list")
        item.onmouseover = function () { this.mouseOver(`BSB-${container}`) }.bind(this)
        item.onmouseout = function () { this.mouseOut(`BSB-${container}`) }.bind(this)

        let outer = document.createXULElement("hbox");
        outer.setAttribute("flex", 1);
        outer.setAttribute("align", "center");

        item.appendChild(outer);

        let userContextIcon = document.createXULElement("hbox");
        userContextIcon.className = "userContext-icon";
        userContextIcon.setAttribute("width", 24);
        userContextIcon.setAttribute("height", 24);
        userContextIcon.classList.add("userContext-icon-inprefs");
        outer.appendChild(userContextIcon);

        let label = document.createXULElement("label");
        label.setAttribute("flex", 1);
        label.classList.add("bsb_label")
        let BSBName = ""
        this.setURL(this.BSBs.data[container].url, label)
        outer.appendChild(label);

        let containerButtons = document.createXULElement("hbox");
        containerButtons.className = "container-buttons";
        item.appendChild(containerButtons);

        let prefsButton = document.createXULElement("button");
        prefsButton.addEventListener("command", function (event) {
          gSubDialog.open(
            "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
            undefined,
            { id: event.target.getAttribute("value"), new: false }
          );
        });
        prefsButton.setAttribute("value", container);
        document.l10n.setAttributes(prefsButton, "sidebar2-pref-setting");
        containerButtons.appendChild(prefsButton);

        let removeButton = document.createXULElement("button");
        removeButton.addEventListener("command", function (event) {
          this.deleteWebpanel(event.target.getAttribute("value"))
        }.bind(this));
        removeButton.setAttribute("value", container);
        document.l10n.setAttributes(removeButton, "sidebar2-pref-delete");
        containerButtons.appendChild(removeButton);
        this._list.insertBefore(item, document.getElementById("BSBSpace"));
      } else { 
        this._list.insertBefore(document.getElementById(`BSB-${container}`), document.getElementById("BSBSpace")); 
        this.setURL(this.BSBs.data[container].url, document.getElementById(`BSB-${container}`).querySelector(".bsb_label")) }
    }
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


