/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ObservableObject = require("devtools/shared/observable-object");
const {getDeviceFront} = require("devtools/server/actors/device");
const {Connection} = require("devtools/client/connection-manager");

const {Cu} = require("chrome");

const _knownDeviceStores = new WeakMap();

var DeviceStore;

module.exports = DeviceStore = function(connection) {
  // If we already know about this connection,
  // let's re-use the existing store.
  if (_knownDeviceStores.has(connection)) {
    return _knownDeviceStores.get(connection);
  }

  _knownDeviceStores.set(connection, this);

  ObservableObject.call(this, {});

  this._resetStore();

  this.destroy = this.destroy.bind(this);
  this._onStatusChanged = this._onStatusChanged.bind(this);

  this._connection = connection;
  this._connection.once(Connection.Events.DESTROYED, this.destroy);
  this._connection.on(Connection.Events.STATUS_CHANGED, this._onStatusChanged);
  this._onTabListChanged = this._onTabListChanged.bind(this);
  this._onStatusChanged();
  return this;
};

DeviceStore.prototype = {
  destroy: function() {
    if (this._connection) {
      // While this.destroy is bound using .once() above, that event may not
      // have occurred when the DeviceStore client calls destroy, so we
      // manually remove it here.
      this._connection.off(Connection.Events.DESTROYED, this.destroy);
      this._connection.off(Connection.Events.STATUS_CHANGED, this._onStatusChanged);
      _knownDeviceStores.delete(this._connection);
      this._connection = null;
    }
  },

  _resetStore: function() {
    this.object.description = {};
    this.object.permissions = [];
    this.object.tabs = [];
  },

  _onStatusChanged: function() {
    if (this._connection.status == Connection.Status.CONNECTED) {
      // Watch for changes to remote browser tabs
      this._connection.client.addListener("tabListChanged",
                                          this._onTabListChanged);
      this._listTabs();
    } else {
      if (this._connection.client) {
        this._connection.client.removeListener("tabListChanged",
                                               this._onTabListChanged);
      }
      this._resetStore();
    }
  },

  _onTabListChanged: function() {
    this._listTabs();
  },

  _listTabs: function() {
    if (!this._connection) {
      return;
    }
    this._connection.client.listTabs((resp) => {
      if (resp.error) {
        this._connection.disconnect();
        return;
      }
      this._deviceFront = getDeviceFront(this._connection.client, resp);
      // Save remote browser's tabs
      this.object.tabs = resp.tabs;
      this._feedStore();
    });
  },

  _feedStore: function() {
    this._getDeviceDescription();
    this._getDevicePermissionsTable();
  },

  _getDeviceDescription: function() {
    return this._deviceFront.getDescription()
    .then(json => {
      json.dpi = Math.ceil(json.dpi);
      this.object.description = json;
    });
  },

  _getDevicePermissionsTable: function() {
    return this._deviceFront.getRawPermissionsTable()
    .then(json => {
      let permissionsTable = json.rawPermissionsTable;
      let permissionsArray = [];
      for (let name in permissionsTable) {
        permissionsArray.push({
          name: name,
          app: permissionsTable[name].app,
          trusted: permissionsTable[name].trusted,
          privileged: permissionsTable[name].privileged,
          certified: permissionsTable[name].certified,
        });
      }
      this.object.permissions = permissionsArray;
    });
  }
};
