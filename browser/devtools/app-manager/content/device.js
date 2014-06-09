/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;

const {ConnectionManager, Connection}
  = require("devtools/client/connection-manager");
const {getDeviceFront} = require("devtools/server/actors/device");
const {getTargetForApp, launchApp, closeApp}
  = require("devtools/app-actor-front");
const DeviceStore = require("devtools/app-manager/device-store");
const WebappsStore = require("devtools/app-manager/webapps-store");
const promise = require("devtools/toolkit/deprecated-sync-thenables");
const DEFAULT_APP_ICON = "chrome://browser/skin/devtools/app-manager/default-app-icon.png";

window.addEventListener("message", function(event) {
  try {
    let message = JSON.parse(event.data);
    if (message.name == "connection") {
      let cid = parseInt(message.cid);
      for (let c of ConnectionManager.connections) {
        if (c.uid == cid) {
          UI.connection = c;
          UI.onNewConnection();
          break;
        }
      }
    }
  } catch(e) {
    Cu.reportError(e);
  }
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  UI.destroy();
});

let UI = {
  init: function() {
    this.showFooterIfNeeded();
    this.setTab("apps");
    if (this.connection) {
      this.onNewConnection();
    } else {
      this.hide();
    }
  },

  destroy: function() {
    if (this.connection) {
      this.connection.off(Connection.Events.STATUS_CHANGED, this._onConnectionStatusChange);
    }
    if (this.store) {
      this.store.destroy();
    }
    if (this.template) {
      this.template.destroy();
    }
  },

  showFooterIfNeeded: function() {
    let footer = document.querySelector("#connection-footer");
    if (window.parent == window) {
      // We're alone. Let's add a footer.
      footer.removeAttribute("hidden");
      footer.src = "chrome://browser/content/devtools/app-manager/connection-footer.xhtml";
    } else {
      footer.setAttribute("hidden", "true");
    }
  },

  hide: function() {
    document.body.classList.add("notconnected");
  },

  show: function() {
    document.body.classList.remove("notconnected");
  },

  onNewConnection: function() {
    this.connection.on(Connection.Events.STATUS_CHANGED, this._onConnectionStatusChange);

    this.store = Utils.mergeStores({
      "device": new DeviceStore(this.connection),
      "apps": new WebappsStore(this.connection),
    });

    if (this.template) {
      this.template.destroy();
    }
    this.template = new Template(document.body, this.store, Utils.l10n);

    this.template.start();
    this._onConnectionStatusChange();
  },

  setWallpaper: function(dataurl) {
    document.getElementById("meta").style.backgroundImage = "url(" + dataurl + ")";
  },

  _onConnectionStatusChange: function() {
    if (this.connection.status != Connection.Status.CONNECTED) {
      this.hide();
      this.listTabsResponse = null;
    } else {
      this.show();
      this.connection.client.listTabs(
        response => {
          this.listTabsResponse = response;
          let front = getDeviceFront(this.connection.client, this.listTabsResponse);
          front.getWallpaper().then(longstr => {
            longstr.string().then(dataURL => {
              longstr.release().then(null, Cu.reportError);
              this.setWallpaper(dataURL);
            });
          });
          if (Services.prefs.getBoolPref("devtools.chrome.enabled")) {
            let rootButton = document.getElementById("root-actor-debug");
            if (response.consoleActor) {
              rootButton.removeAttribute("hidden");
            } else {
              rootButton.setAttribute("hidden", "true");
            }
          }
          let tabsButton = document.querySelector(".tab.browser-tabs");
          if (response.tabs.length > 0) {
            tabsButton.classList.remove("hidden");
          } else {
            tabsButton.classList.add("hidden");
          }
        }
      );
    }
  },

  get connected() { return !!this.listTabsResponse; },

  setTab: function(name) {
    var tab = document.querySelector(".tab.selected");
    var panel = document.querySelector(".tabpanel.selected");

    if (tab) tab.classList.remove("selected");
    if (panel) panel.classList.remove("selected");

    var tab = document.querySelector(".tab." + name);
    var panel = document.querySelector(".tabpanel." + name);

    if (tab) tab.classList.add("selected");
    if (panel) panel.classList.add("selected");
  },

  openToolboxForRootActor: function() {
    if (!this.connected) {
      return;
    }

    let options = {
      form: this.listTabsResponse,
      client: this.connection.client,
      chrome: true
    };
    devtools.TargetFactory.forRemoteTab(options).then((target) => {
      top.UI.openAndShowToolboxForTarget(target, "Main process", null);
    });
  },

  openToolboxForApp: function(manifest) {
    if (!this.connected) {
      return;
    }

    let app = this.store.object.apps.all.filter(a => a.manifestURL == manifest)[0];
    getTargetForApp(this.connection.client,
                    this.listTabsResponse.webappsActor,
                    manifest).then((target) => {

      top.UI.openAndShowToolboxForTarget(target, app.name, app.iconURL);
    }, console.error);
  },

  _getTargetForTab: function (form) {
      let options = {
        form: form,
        client: this.connection.client,
        chrome: false
      };
      let deferred = promise.defer();
      return devtools.TargetFactory.forRemoteTab(options);
  },

  openToolboxForTab: function (aNode) {
    let index = Array.prototype.indexOf.apply(
      aNode.parentNode.parentNode.parentNode.children,
      [aNode.parentNode.parentNode]);
    this.connection.client.listTabs(
      response => {
        let tab = response.tabs[index];
        this._getTargetForTab(tab).then(target => {
          top.UI.openAndShowToolboxForTarget(
            target, tab.title, DEFAULT_APP_ICON);
        }, console.error);
      }
    );
  },

  startApp: function(manifest) {
    if (!this.connected) {
      return promise.reject();
    }
    return launchApp(this.connection.client,
                     this.listTabsResponse.webappsActor,
                     manifest);
  },

  stopApp: function(manifest) {
    if (!this.connected) {
      return promise.reject();
    }
    return closeApp(this.connection.client,
                    this.listTabsResponse.webappsActor,
                    manifest);
  },
}

// This must be bound immediately, as it might be used via the message listener
// before UI.init() has been called.
UI._onConnectionStatusChange = UI._onConnectionStatusChange.bind(UI);
