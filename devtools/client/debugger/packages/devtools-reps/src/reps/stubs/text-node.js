/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("testRendering", {
  class: "Text",
  actor: "server1.conn1.child1/obj50",
  preview: {
    kind: "DOMNode",
    nodeType: 3,
    nodeName: "#text",
    textContent: "hello world",
    isConnected: true,
  },
});
stubs.set("testRenderingDisconnected", {
  class: "Text",
  actor: "server1.conn1.child1/obj50",
  preview: {
    kind: "DOMNode",
    nodeType: 3,
    nodeName: "#text",
    textContent: "hello world",
    isConnected: false,
  },
});
stubs.set("testRenderingWithEOL", {
  class: "Text",
  actor: "server1.conn1.child1/obj50",
  preview: {
    kind: "DOMNode",
    nodeType: 3,
    nodeName: "#text",
    textContent: "hello\nworld",
  },
});

module.exports = stubs;
