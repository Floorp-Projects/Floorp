/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Document", {
  type: "object",
  class: "HTMLDocument",
  actor: "server1.conn17.obj115",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMNode",
    nodeType: 9,
    nodeName: "#document",
    location: "https://www.mozilla.org/en-US/firefox/new/",
  },
});

stubs.set("Location-less Document", {
  type: "object",
  actor: "server1.conn6.child1/obj31",
  class: "HTMLDocument",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  preview: {
    kind: "DOMNode",
    nodeType: 9,
    nodeName: "#document",
  },
});

module.exports = stubs;
