/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();

stubs.set("getter", {
  configurable: true,
  enumerable: true,
  get: {
    type: "object",
    actor: "server2.conn1.child1/obj106",
    class: "Function",
    extensible: true,
    frozen: false,
    sealed: false,
    name: "get x",
    displayName: "get x",
    location: {
      url: "debugger eval code",
      line: 1,
    },
  },
  set: {
    type: "undefined",
  },
});

stubs.set("setter", {
  configurable: true,
  enumerable: true,
  get: {
    type: "undefined",
  },
  set: {
    type: "object",
    actor: "server2.conn1.child1/obj116",
    class: "Function",
    extensible: true,
    frozen: false,
    sealed: false,
    name: "set x",
    displayName: "set x",
    location: {
      url: "debugger eval code",
      line: 1,
    },
  },
});

stubs.set("getter setter", {
  configurable: true,
  enumerable: true,
  get: {
    type: "object",
    actor: "server2.conn1.child1/obj127",
    class: "Function",
    extensible: true,
    frozen: false,
    sealed: false,
    name: "get x",
    displayName: "get x",
    location: {
      url: "debugger eval code",
      line: 1,
    },
  },
  set: {
    type: "object",
    actor: "server2.conn1.child1/obj128",
    class: "Function",
    extensible: true,
    frozen: false,
    sealed: false,
    name: "set x",
    displayName: "set x",
    location: {
      url: "debugger eval code",
      line: 1,
    },
  },
});
module.exports = stubs;
