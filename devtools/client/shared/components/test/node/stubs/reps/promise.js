/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const stubs = new Map();
stubs.set("Pending", {
  type: "object",
  actor: "server1.conn1.child1/obj54",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "pending",
      },
    },
    ownPropertiesLength: 1,
  },
});

stubs.set("FulfilledWithNumber", {
  type: "object",
  actor: "server1.conn1.child1/obj55",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
        value: 42,
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("FulfilledWithString", {
  type: "object",
  actor: "server1.conn1.child1/obj56",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
        value: "foo",
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("FulfilledWithObject", {
  type: "object",
  actor: "server1.conn1.child1/obj59",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
        value: {
          type: "object",
          actor: "server1.conn1.child1/obj60",
          class: "Object",
          extensible: true,
          frozen: false,
          sealed: false,
          ownPropertyLength: 2,
        },
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("FulfilledWithArray", {
  type: "object",
  actor: "server1.conn1.child1/obj57",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
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
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("FulfilledWithNode", {
  type: "object",
  actor: "server1.conn1.child1/obj217",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
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
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("FulfilledWithDisconnectedNode", {
  type: "object",
  actor: "server1.conn1.child1/obj217",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "fulfilled",
      },
      "<value>": {
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
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("RejectedWithNumber", {
  type: "object",
  actor: "server0.conn0.child3/obj27",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "rejected",
      },
      "<reason>": {
        value: 123,
      },
    },
    ownPropertiesLength: 2,
  },
});

stubs.set("RejectedWithObject", {
  type: "object",
  actor: "server0.conn0.child3/obj67",
  class: "Promise",
  ownPropertyLength: 0,
  preview: {
    kind: "Object",
    ownProperties: {
      "<state>": {
        value: "rejected",
      },
      "<reason>": {
        value: {
          type: "object",
          actor: "server1.conn1.child1/obj68",
          class: "Object",
          extensible: true,
          frozen: false,
          sealed: false,
          ownPropertyLength: 1,
        },
      },
    },
    ownPropertiesLength: 2,
  },
});

module.exports = stubs;
