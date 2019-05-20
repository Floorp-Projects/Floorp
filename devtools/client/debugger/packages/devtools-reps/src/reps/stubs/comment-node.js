/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Comment", {
  type: "object",
  actor: "server1.conn1.child1/obj47",
  class: "Comment",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "DOMNode",
    nodeType: 8,
    nodeName: "#comment",
    textContent:
      "test\nand test\nand test\nand test\nand test\nand test\nand test",
  },
});

module.exports = stubs;
