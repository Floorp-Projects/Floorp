/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ObservableObject = require("devtools/shared/observable-object");
const promise = require("sdk/core/promise");
const {Connection} = require("devtools/client/connection-manager");

const {Cu} = require("chrome");
const dbgClient = Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
const _knownWebappsStores = new WeakMap();

let WebappsStore;

module.exports = WebappsStore = function(connection) {
  // If we already know about this connection,
  // let's re-use the existing store.
  if (_knownWebappsStores.has(connection)) {
    return _knownWebappsStores.get(connection);
  }

  _knownWebappsStores.set(connection, this);

  ObservableObject.call(this, {});

  this._resetStore();

  this._destroy = this._destroy.bind(this);
  this._onStatusChanged = this._onStatusChanged.bind(this);

  this._connection = connection;
  this._connection.once(Connection.Events.DESTROYED, this._destroy);
  this._connection.on(Connection.Events.STATUS_CHANGED, this._onStatusChanged);
  this._onStatusChanged();
  return this;
}

WebappsStore.prototype = {
  _destroy: function() {
    this._connection.off(Connection.Events.STATUS_CHANGED, this._onStatusChanged);
    _knownWebappsStores.delete(this._connection);
    this._connection = null;
  },

  _resetStore: function() {
    this.object.all = []; // list of app objects
    this.object.running = []; // list of manifests
  },

  _getAppFromManifest: function(manifest) {
    for (let app of this.object.all) {
      if (app.manifestURL == manifest) {
        return app;
      }
    }
    return null;
  },

  _onStatusChanged: function() {
    if (this._connection.status == Connection.Status.CONNECTED) {
      this._listTabs();
    } else {
      this._resetStore();
    }
  },

  _listTabs: function() {
    this._connection.client.listTabs((resp) => {
      this._webAppsActor = resp.webappsActor;
      this._feedStore();
    });
  },

  _feedStore: function(deviceFront, webAppsActor) {
    this._listenToApps();
    this._getAllApps()
    .then(this._getRunningApps.bind(this))
    .then(this._getAppsIcons.bind(this))
  },

  _listenToApps: function() {
    let deferred = promise.defer();
    let client = this._connection.client;

    let request = {
      to: this._webAppsActor,
      type: "watchApps"
    };

    client.request(request, (res) => {
      if (res.error) {
        return deferred.reject(res.error);
      }

      client.addListener("appOpen", (type, { manifestURL }) => {
        this._onAppOpen(manifestURL);
      });

      client.addListener("appClose", (type, { manifestURL }) => {
        this._onAppClose(manifestURL);
      });

      client.addListener("appInstall", (type, { manifestURL }) => {
        this._onAppInstall(manifestURL);
      });

      client.addListener("appUninstall", (type, { manifestURL }) => {
        this._onAppUninstall(manifestURL);
      });

      return deferred.resolve();
    })
    return deferred.promise;
  },

  _getAllApps: function() {
    let deferred = promise.defer();
    let request = {
      to: this._webAppsActor,
      type: "getAll"
    };

    this._connection.client.request(request, (res) => {
      if (res.error) {
        return deferred.reject(res.error);
      }
      let apps = res.apps;
      for (let a of apps) {
        a.running = false;
      }
      this.object.all = apps;
      return deferred.resolve();
    });
    return deferred.promise;
  },

  _getRunningApps: function() {
    let deferred = promise.defer();
    let request = {
      to: this._webAppsActor,
      type: "listRunningApps"
    };

    this._connection.client.request(request, (res) => {
      if (res.error) {
        return deferred.reject(res.error);
      }

      let manifests = res.apps;
      this.object.running = manifests;

      for (let m of manifests) {
        let a = this._getAppFromManifest(m);
        if (a) {
          a.running = true;
        } else {
          return deferred.reject("Unexpected manifest: " + m);
        }
      }

      return deferred.resolve();
    });
    return deferred.promise;
  },

  _getAppsIcons: function() {
    let deferred = promise.defer();
    let allApps = this.object.all;

    let request = {
      to: this._webAppsActor,
      type: "getIconAsDataURL"
    };

    let client = this._connection.client;

    let idx = 0;
    (function getIcon() {
      if (idx == allApps.length) {
        return deferred.resolve();
      }
      let a = allApps[idx++];
      request.manifestURL = a.manifestURL;
      return client.request(request, (res) => {
        if (res.error) {
          Cu.reportError(res.message || res.error);
        }

        if (res.url) {
          a.iconURL = res.url;
        }
        getIcon();
      });
    })();

    return deferred.promise;
  },

  _onAppOpen: function(manifest) {
    let a = this._getAppFromManifest(manifest);
    a.running = true;
    let running = this.object.running;
    if (running.indexOf(manifest) < 0) {
      this.object.running.push(manifest);
    }
  },

  _onAppClose: function(manifest) {
    let a = this._getAppFromManifest(manifest);
    a.running = false;
    let running = this.object.running;
    this.object.running = running.filter((m) => {
      return m != manifest;
    });
  },

  _onAppInstall: function(manifest) {
    let client = this._connection.client;
    let request = {
      to: this._webAppsActor,
      type: "getApp",
      manifestURL: manifest
    };

    client.request(request, (res) => {
      if (res.error) {
        if (res.error == "forbidden") {
          // We got a notification for an app we don't have access to.
          // Ignore.
          return;
        }
        Cu.reportError(res.message || res.error);
        return;
      }

      let app = res.app;
      app.running = false;

      let notFound = true;
      let proxifiedApp;
      for (let i = 0; i < this.object.all.length; i++) {
        let storedApp = this.object.all[i];
        if (storedApp.manifestURL == app.manifestURL) {
          this.object.all[i] = app;
          proxifiedApp = this.object.all[i];
          notFound = false;
          break;
        }
      }
      if (notFound) {
        this.object.all.push(app);
        proxifiedApp = this.object.all[this.object.all.length - 1];
      }

      request.type = "getIconAsDataURL";
      client.request(request, (res) => {
        if (res.url) {
          proxifiedApp.iconURL = res.url;
        }
      });

      // This app may have been running while being installed, so check the list
      // of running apps again to get the right answer.
      this._getRunningApps();
    });
  },

  _onAppUninstall: function(manifest) {
    this.object.all = this.object.all.filter((app) => {
      return (app.manifestURL != manifest);
    });
  },
}
