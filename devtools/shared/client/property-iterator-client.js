/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");

/**
 * A PropertyIteratorClient provides a way to access to property names and
 * values of an object efficiently, slice by slice.
 * Note that the properties can be sorted in the backend,
 * this is controled while creating the PropertyIteratorClient
 * from ObjectClient.enumProperties.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A PropertyIteratorActor grip returned by the protocol via
 *        BrowsingContextTargetActor.enumProperties request.
 */
function PropertyIteratorClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}

PropertyIteratorClient.prototype = {
  get actor() {
    return this._grip.actor;
  },

  /**
   * Get the total number of properties available in the iterator.
   */
  get count() {
    return this._grip.count;
  },

  /**
   * Get one or more property names that correspond to the positions in the
   * indexes parameter.
   *
   * @param indexes Array
   *        An array of property indexes.
   * @param callback Function
   *        The function called when we receive the property names.
   */
  names: DebuggerClient.requester({
    type: "names",
    indexes: arg(0)
  }, {}),

  /**
   * Get a set of following property value(s).
   *
   * @param start Number
   *        The index of the first property to fetch.
   * @param count Number
   *        The number of properties to fetch.
   * @param callback Function
   *        The function called when we receive the property values.
   */
  slice: DebuggerClient.requester({
    type: "slice",
    start: arg(0),
    count: arg(1)
  }, {}),

  /**
   * Get all the property values.
   *
   * @param callback Function
   *        The function called when we receive the property values.
   */
  all: DebuggerClient.requester({
    type: "all"
  }, {}),
};

module.exports = PropertyIteratorClient;
