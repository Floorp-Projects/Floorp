/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("html", {
  type: "object",
  actor: "server1.conn7.child1/obj195",
  class: "DocumentType",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 10,
    nodeName: "html",
    isConnected: true,
  },
});

stubs.set("unnamed", {
  type: "object",
  actor: "server1.conn7.child1/obj195",
  class: "DocumentType",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 10,
    nodeName: "",
    isConnected: true,
  },
});

module.exports = stubs;
