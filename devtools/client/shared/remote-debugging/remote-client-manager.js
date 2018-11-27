/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This class is designed to be a singleton shared by all DevTools to get access to
 * existing clients created for remote debugging.
 */
class RemoteClientManager {
  constructor() {
    this._clients = new Map();
    this._onClientClosed = this._onClientClosed.bind(this);
  }

  /**
   * Store a remote client that is already connected.
   *
   * @param {String} id
   *        Remote runtime id (see devtools/client/aboutdebugging-new/src/types).
   * @param {String} type
   *        Remote runtime type (see devtools/client/aboutdebugging-new/src/types).
   * @param {DebuggerClient} client
   */
  setClient(id, type, client) {
    const key = this._getKey(id, type);
    this._clients.set(key, client);
    client.addOneTimeListener("closed", this._onClientClosed);
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
    const key = decodeURIComponent(remoteId);
    return this._clients.get(key);
  }

  _getKey(id, type) {
    return id + "-" + type;
  }

  _removeClientByKey(key) {
    const client = this._clients.get(key);
    if (client) {
      client.removeListener("closed", this._onClientClosed);
      this._clients.delete(key);
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
exports.remoteClientManager = new RemoteClientManager();
