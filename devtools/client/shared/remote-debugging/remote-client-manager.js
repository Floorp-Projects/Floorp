/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECTION_TYPES,
} = require("devtools/client/shared/remote-debugging/constants");

/**
 * This class is designed to be a singleton shared by all DevTools to get access to
 * existing clients created for remote debugging.
 */
class RemoteClientManager {
  constructor() {
    this._clients = new Map();
    this._runtimeInfoMap = new Map();
    this._onClientClosed = this._onClientClosed.bind(this);
  }

  /**
   * Store a remote client that is already connected.
   *
   * @param {String} id
   *        Remote runtime id (see devtools/client/aboutdebugging/src/types).
   * @param {String} type
   *        Remote runtime type (see devtools/client/aboutdebugging/src/types).
   * @param {DebuggerClient} client
   * @param {Object} runtimeInfo
   *        See runtimeInfo type from client/aboutdebugging/src/types/runtime.js
   */
  setClient(id, type, client, runtimeInfo) {
    const key = this._getKey(id, type);
    this._clients.set(key, client);
    if (runtimeInfo) {
      this._runtimeInfoMap.set(key, runtimeInfo);
    }
    client.once("closed", this._onClientClosed);
  }

  // See JSDoc for id, type from setClient.
  hasClient(id, type) {
    return this._clients.has(this._getKey(id, type));
  }

  // See JSDoc for id, type from setClient.
  getClient(id, type) {
    return this._clients.get(this._getKey(id, type));
  }

  // See JSDoc for id, type from setClient.
  removeClient(id, type) {
    const key = this._getKey(id, type);
    this._removeClientByKey(key);
  }

  removeAllClients() {
    const keys = [...this._clients.keys()];
    for (const key of keys) {
      this._removeClientByKey(key);
    }
  }

  /**
   * Retrieve a unique, url-safe key based on a runtime id and type.
   */
  getRemoteId(id, type) {
    return encodeURIComponent(this._getKey(id, type));
  }

  /**
   * Retrieve a managed client for a remote id. The remote id should have been generated
   * using getRemoteId.
   */
  getClientByRemoteId(remoteId) {
    const key = this._getKeyByRemoteId(remoteId);
    return this._clients.get(key);
  }

  /**
   * Retrieve the runtime info for a remote id. To display metadata about a runtime, such
   * as name, device name, version... this runtimeInfo should be used rather than calling
   * APIs on the client.
   */
  getRuntimeInfoByRemoteId(remoteId) {
    const key = this._getKeyByRemoteId(remoteId);
    return this._runtimeInfoMap.get(key);
  }

  /**
   * Retrieve a managed client for a remote id. The remote id should have been generated
   * using getRemoteId.
   */
  getConnectionTypeByRemoteId(remoteId) {
    const key = this._getKeyByRemoteId(remoteId);
    for (const type of Object.values(CONNECTION_TYPES)) {
      if (key.endsWith(type)) {
        return type;
      }
    }
    return CONNECTION_TYPES.UNKNOWN;
  }

  _getKey(id, type) {
    return id + "-" + type;
  }

  _getKeyByRemoteId(remoteId) {
    if (!remoteId) {
      // If no remote id was provided, return the key corresponding to the local
      // this-firefox runtime.
      const { THIS_FIREFOX } = CONNECTION_TYPES;
      return this._getKey(THIS_FIREFOX, THIS_FIREFOX);
    }

    return decodeURIComponent(remoteId);
  }

  _removeClientByKey(key) {
    const client = this._clients.get(key);
    if (client) {
      client.off("closed", this._onClientClosed);
      this._clients.delete(key);
      this._runtimeInfoMap.delete(key);
    }
  }

  /**
   * Cleanup all closed clients when a "closed" notification is received from a client.
   */
  _onClientClosed() {
    const closedClientKeys = [...this._clients.keys()].filter(key => {
      return this._clients.get(key)._closed;
    });

    for (const key of closedClientKeys) {
      this._removeClientByKey(key);
    }
  }
}

// Expose a singleton of RemoteClientManager.
module.exports = {
  remoteClientManager: new RemoteClientManager(),
};
