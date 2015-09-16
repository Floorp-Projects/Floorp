/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ObservableObject = require("devtools/shared/observable-object");
const {Connection} = require("devtools/client/connection-manager");

const _knownConnectionStores = new WeakMap();

var ConnectionStore;

module.exports = ConnectionStore = function(connection) {
  // If we already know about this connection,
  // let's re-use the existing store.
  if (_knownConnectionStores.has(connection)) {
    return _knownConnectionStores.get(connection);
  }
  _knownConnectionStores.set(connection, this);

  ObservableObject.call(this, {status:null,host:null,port:null});

  this.destroy = this.destroy.bind(this);
  this._feedStore = this._feedStore.bind(this);

  this._connection = connection;
  this._connection.once(Connection.Events.DESTROYED, this.destroy);
  this._connection.on(Connection.Events.STATUS_CHANGED, this._feedStore);
  this._connection.on(Connection.Events.PORT_CHANGED, this._feedStore);
  this._connection.on(Connection.Events.HOST_CHANGED, this._feedStore);
  this._feedStore();
  return this;
}

ConnectionStore.prototype = {
  destroy: function() {
    if (this._connection) {
      // While this.destroy is bound using .once() above, that event may not
      // have occurred when the ConnectionStore client calls destroy, so we
      // manually remove it here.
      this._connection.off(Connection.Events.DESTROYED, this.destroy);
      this._connection.off(Connection.Events.STATUS_CHANGED, this._feedStore);
      this._connection.off(Connection.Events.PORT_CHANGED, this._feedStore);
      this._connection.off(Connection.Events.HOST_CHANGED, this._feedStore);
      _knownConnectionStores.delete(this._connection);
      this._connection = null;
    }
  },

  _feedStore: function() {
    this.object.status = this._connection.status;
    this.object.host = this._connection.host;
    this.object.port = this._connection.port;
  }
}
