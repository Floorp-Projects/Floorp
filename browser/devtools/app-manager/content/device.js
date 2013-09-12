/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;

const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const {getDeviceFront} = require("devtools/server/actors/device");
const DeviceStore = require("devtools/app-manager/device-store");
const WebappsStore = require("devtools/app-manager/webapps-store");
const promise = require("sdk/core/promise");

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
}, false);

let UI = {
  init: function() {
    this.showFooterIfNeeded();
    this._onConnectionStatusChange = this._onConnectionStatusChange.bind(this);
    this.setTab("apps");
    if (this.connection) {
      this.onNewConnection();
    } else {
      this.hide();
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
        }
      );
    }
  },

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

  screenshot: function() {
    if (!this.listTabsResponse)
      return;

    let front = getDeviceFront(this.connection.client, this.listTabsResponse);
    front.screenshotToBlob().then(blob => {
      let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
      let gBrowser = topWindow.gBrowser;
      let url = topWindow.URL.createObjectURL(blob);
      let tab = gBrowser.selectedTab = gBrowser.addTab(url);
      tab.addEventListener("TabClose", function onTabClose() {
        tab.removeEventListener("TabClose", onTabClose, false);
        topWindow.URL.revokeObjectURL(url);
      }, false);
    }).then(null, console.error);
  },

  _getTargetForApp: function(manifest) { // FIXME <- will be implemented in bug 912476
    if (!this.listTabsResponse)
      return null;

    let actor = this.listTabsResponse.webappsActor;
    let deferred = promise.defer();
    let request = {
      to: actor,
      type: "getAppActor",
      manifestURL: manifest,
    }
    this.connection.client.request(request, (res) => {
      if (res.error) {
        deferred.reject(res.error);
      } else {
        let options = {
          form: res.actor,
          client: this.connection.client,
          chrome: false
        };

        devtools.TargetFactory.forRemoteTab(options).then((target) => {
          deferred.resolve(target)
        }, (error) => {
          deferred.reject(error);
        });
      }
    });
    return deferred.promise;
  },

  openToolbox: function(manifest) {
    this._getTargetForApp(manifest).then((target) => {
      gDevTools.showToolbox(target,
                            null,
                            devtools.Toolbox.HostType.WINDOW).then(toolbox => {
        this.connection.once(Connection.Events.DISCONNECTED, () => {
          toolbox.destroy();
        });
      });
    }, console.error);
  },

  startApp: function(manifest) {
    let deferred = promise.defer();

    if (!this.listTabsResponse) {
      deferred.reject();
    } else {
      let actor = this.listTabsResponse.webappsActor;
      let request = {
        to: actor,
        type: "launch",
        manifestURL: manifest,
      }
      this.connection.client.request(request, (res) => {
        deferred.resolve()
      });
    }
    return deferred.promise;
  },

  stopApp: function(manifest) {
    let deferred = promise.defer();

    if (!this.listTabsResponse) {
      deferred.reject();
    } else {
      let actor = this.listTabsResponse.webappsActor;
      let request = {
        to: actor,
        type: "close",
        manifestURL: manifest,
      }
      this.connection.client.request(request, (res) => {
        deferred.resolve()
      });
    }
    return deferred.promise;
  },
}
