/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Document", {
  actor: "server1.conn1.child1/accessible31",
  typeName: "accessible",
  preview: {
    name: "New Tab",
    role: "document",
    isConnected: true,
  },
});

stubs.set("ButtonMenu", {
  actor: "server1.conn1.child1/accessible38",
  typeName: "accessible",
  preview: {
    name: "New to Nightly? Let’s get started.",
    role: "buttonmenu",
    isConnected: true,
  },
});

stubs.set("NoName", {
  actor: "server1.conn1.child1/accessible93",
  typeName: "accessible",
  preview: {
    name: null,
    role: "text container",
    isConnected: true,
  },
});

stubs.set("NoPreview", {
  actor: "server1.conn1.child1/accessible93",
  typeName: "accessible",
});

stubs.set("DisconnectedAccessible", {
  actor: null,
  typeName: "accessible",
  preview: {
    name: null,
    role: "section",
    isConnected: false,
  },
});

const name = "a".repeat(1000);
stubs.set("AccessibleWithLongName", {
  actor: "server1.conn1.child1/accessible98",
  typeName: "accessible",
  preview: {
    name,
    role: "text leaf",
    isConnected: true,
  },
});

stubs.set("PushButton", {
  actor: "server1.conn1.child1/accessible157",
  typeName: "accessible",
  preview: {
    name: "Search",
    role: "pushbutton",
    isConnected: true,
  },
});

module.exports = stubs;
