/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  arg,
  DebuggerClient,
} = require("devtools/shared/client/debugger-client");
loader.lazyRequireGetter(
  this,
  "PropertyIteratorClient",
  "devtools/shared/client/property-iterator-client"
);
loader.lazyRequireGetter(
  this,
  "SymbolIteratorClient",
  "devtools/shared/client/symbol-iterator-client"
);

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip object
 *        A pause-lifetime object grip returned by the protocol.
 */
function ObjectClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}

ObjectClient.prototype = {
  get actor() {
    return this._grip.actor;
  },
  get _transport() {
    return this._client._transport;
  },

  valid: true,

  get isFrozen() {
    return this._grip.frozen;
  },
  get isSealed() {
    return this._grip.sealed;
  },
  get isExtensible() {
    return this._grip.extensible;
  },

  threadGrip: DebuggerClient.requester({
    type: "threadGrip",
  }),

  getDefinitionSite: DebuggerClient.requester(
    {
      type: "definitionSite",
    },
    {
      before: function(packet) {
        if (this._grip.class != "Function") {
          throw new Error(
            "getDefinitionSite is only valid for function grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the names of a function's formal parameters.
   *
   * @param onResponse function
   *        Called with an object of the form:
   *        { parameterNames:[<parameterName>, ...] }
   *        where each <parameterName> is the name of a parameter.
   */
  getParameterNames: DebuggerClient.requester(
    {
      type: "parameterNames",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Function") {
          throw new Error(
            "getParameterNames is only valid for function grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   *
   * @param onResponse function Called with the request's response.
   */
  getOwnPropertyNames: DebuggerClient.requester({
    type: "ownPropertyNames",
  }),

  /**
   * Request the prototype and own properties of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getPrototypeAndProperties: DebuggerClient.requester({
    type: "prototypeAndProperties",
  }),

  /**
   * Request a PropertyIteratorClient instance to ease listing
   * properties for this object.
   *
   * @param options Object
   *        A dictionary object with various boolean attributes:
   *        - ignoreIndexedProperties Boolean
   *          If true, filters out Array items.
   *          e.g. properties names between `0` and `object.length`.
   *        - ignoreNonIndexedProperties Boolean
   *          If true, filters out items that aren't array items
   *          e.g. properties names that are not a number between `0`
   *          and `object.length`.
   *        - sort Boolean
   *          If true, the iterator will sort the properties by name
   *          before dispatching them.
   * @param onResponse function Called with the client instance.
   */
  enumProperties: DebuggerClient.requester(
    {
      type: "enumProperties",
      options: arg(0),
    },
    {
      after: function(response) {
        if (response.iterator) {
          return {
            iterator: new PropertyIteratorClient(
              this._client,
              response.iterator
            ),
          };
        }
        return response;
      },
    }
  ),

  /**
   * Request a PropertyIteratorClient instance to enumerate entries in a
   * Map/Set-like object.
   *
   * @param onResponse function Called with the request's response.
   */
  enumEntries: DebuggerClient.requester(
    {
      type: "enumEntries",
    },
    {
      before: function(packet) {
        if (
          !["Map", "WeakMap", "Set", "WeakSet", "Storage"].includes(
            this._grip.class
          )
        ) {
          throw new Error(
            "enumEntries is only valid for Map/Set/Storage-like grips."
          );
        }
        return packet;
      },
      after: function(response) {
        if (response.iterator) {
          return {
            iterator: new PropertyIteratorClient(
              this._client,
              response.iterator
            ),
          };
        }
        return response;
      },
    }
  ),

  /**
   * Request a SymbolIteratorClient instance to enumerate symbols in an object.
   *
   * @param onResponse function Called with the request's response.
   */
  enumSymbols: DebuggerClient.requester(
    {
      type: "enumSymbols",
    },
    {
      before: function(packet) {
        if (this._grip.type !== "object") {
          throw new Error("enumSymbols is only valid for objects grips.");
        }
        return packet;
      },
      after: function(response) {
        if (response.iterator) {
          return {
            iterator: new SymbolIteratorClient(this._client, response.iterator),
          };
        }
        return response;
      },
    }
  ),

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param name string The name of the requested property.
   * @param onResponse function Called with the request's response.
   */
  getProperty: DebuggerClient.requester({
    type: "property",
    name: arg(0),
  }),

  /**
   * Request the value of the object's specified property.
   *
   * @param name string The name of the requested property.
   * @param receiverId string|null The actorId of the receiver to be used for getters.
   * @param onResponse function Called with the request's response.
   */
  getPropertyValue: DebuggerClient.requester({
    type: "propertyValue",
    name: arg(0),
    receiverId: arg(1),
  }),

  /**
   * Request the prototype of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getPrototype: DebuggerClient.requester({
    type: "prototype",
  }),

  /**
   * Evaluate a callable object with context and arguments.
   *
   * @param context any The value to use as the function context.
   * @param arguments Array<any> An array of values to use as the function's arguments.
   * @param onResponse function Called with the request's response.
   */
  apply: DebuggerClient.requester({
    type: "apply",
    context: arg(0),
    arguments: arg(1),
  }),

  /**
   * Request the display string of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getDisplayString: DebuggerClient.requester({
    type: "displayString",
  }),

  /**
   * Request the scope of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getScope: DebuggerClient.requester(
    {
      type: "scope",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Function") {
          throw new Error("scope is only valid for function grips.");
        }
        return packet;
      },
    }
  ),

  /**
   * Request the promises directly depending on the current promise.
   */
  getDependentPromises: DebuggerClient.requester(
    {
      type: "dependentPromises",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Promise") {
          throw new Error(
            "getDependentPromises is only valid for promise " + "grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the stack to the promise's allocation point.
   */
  getPromiseAllocationStack: DebuggerClient.requester(
    {
      type: "allocationStack",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Promise") {
          throw new Error(
            "getAllocationStack is only valid for promise grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the stack to the promise's fulfillment point.
   */
  getPromiseFulfillmentStack: DebuggerClient.requester(
    {
      type: "fulfillmentStack",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Promise") {
          throw new Error(
            "getPromiseFulfillmentStack is only valid for " + "promise grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the stack to the promise's rejection point.
   */
  getPromiseRejectionStack: DebuggerClient.requester(
    {
      type: "rejectionStack",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Promise") {
          throw new Error(
            "getPromiseRejectionStack is only valid for " + "promise grips."
          );
        }
        return packet;
      },
    }
  ),

  /**
   * Request the target and handler internal slots of a proxy.
   */
  getProxySlots: DebuggerClient.requester(
    {
      type: "proxySlots",
    },
    {
      before: function(packet) {
        if (this._grip.class !== "Proxy") {
          throw new Error("getProxySlots is only valid for proxy grips.");
        }
        return packet;
      },
      after: function(response) {
        // Before Firefox 68 (bug 1392760), the proxySlots request didn't exist.
        // The proxy target and handler were directly included in the grip.
        if (response.error === "unrecognizedPacketType") {
          const { proxyTarget, proxyHandler } = this._grip;
          return { proxyTarget, proxyHandler };
        }
        return response;
      },
    }
  ),
  addWatchpoint: DebuggerClient.requester({
    type: "addWatchpoint",
    property: arg(0),
    label: arg(1),
    watchpointType: arg(2),
  }),
  removeWatchpoint: DebuggerClient.requester({
    type: "removeWatchpoint",
    property: arg(0),
  }),
};

module.exports = ObjectClient;
