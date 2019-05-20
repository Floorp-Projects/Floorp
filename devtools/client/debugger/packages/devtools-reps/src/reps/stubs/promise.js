/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Pending", {
  type: "object",
  actor: "server1.conn1.child1/obj54",
  class: "Promise",
  promiseState: {
    state: "pending",
    creationTimestamp: 1477327760242.5752,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithNumber", {
  type: "object",
  actor: "server1.conn1.child1/obj55",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: 42,
    creationTimestamp: 1477327760242.721,
    timeToSettle: 0.018497000000479602,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithString", {
  type: "object",
  actor: "server1.conn1.child1/obj56",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: "foo",
    creationTimestamp: 1477327760243.2483,
    timeToSettle: 0.0019969999998465937,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithObject", {
  type: "object",
  actor: "server1.conn1.child1/obj59",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: {
      type: "object",
      actor: "server1.conn1.child1/obj60",
      class: "Object",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 2,
    },
    creationTimestamp: 1477327760243.2214,
    timeToSettle: 0.002035999999861815,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithArray", {
  type: "object",
  actor: "server1.conn1.child1/obj57",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: {
      type: "object",
      actor: "server1.conn1.child1/obj58",
      class: "Array",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 4,
      preview: {
        kind: "ArrayLike",
        length: 3,
      },
    },
    creationTimestamp: 1477327760242.9597,
    timeToSettle: 0.006158000000141328,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithNode", {
  type: "object",
  actor: "server1.conn1.child1/obj217",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: {
      type: "object",
      actor: "server1.conn1.child1/obj218",
      class: "HTMLButtonElement",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 0,
      preview: {
        kind: "DOMNode",
        nodeType: 1,
        nodeName: "button",
        isConnected: true,
        attributes: {
          id: "btn-1",
          class: "btn btn-log",
          type: "button",
        },
        attributesLength: 3,
      },
    },
    creationTimestamp: 1480423091620.3716,
    timeToSettle: 0.02842400000372436,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

stubs.set("FulfilledWithDisconnectedNode", {
  type: "object",
  actor: "server1.conn1.child1/obj217",
  class: "Promise",
  promiseState: {
    state: "fulfilled",
    value: {
      type: "object",
      actor: "server1.conn1.child1/obj218",
      class: "HTMLButtonElement",
      extensible: true,
      frozen: false,
      sealed: false,
      ownPropertyLength: 0,
      preview: {
        kind: "DOMNode",
        nodeType: 1,
        nodeName: "button",
        isConnected: false,
        attributes: {
          id: "btn-1",
          class: "btn btn-log",
          type: "button",
        },
        attributesLength: 3,
      },
    },
    creationTimestamp: 1480423091620.3716,
    timeToSettle: 0.02842400000372436,
  },
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
});

module.exports = stubs;
