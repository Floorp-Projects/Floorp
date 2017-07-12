/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

class ObjectClient {
  constructor(client, grip) {
    this._grip = grip;
    this._client = client;
    this.request = this._client.request;
  }

  getPrototypeAndProperties() {
    return this._client.request({
      to: this._grip.actor,
      type: "prototypeAndProperties"
    });
  }
}

module.exports = { ObjectClient };
