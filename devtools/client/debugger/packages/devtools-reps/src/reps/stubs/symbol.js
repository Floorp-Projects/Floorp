/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Symbol", {
  type: "symbol",
  actor: "server1.conn1.child1/symbol1",
  name: "foo",
});

stubs.set("SymbolWithoutIdentifier", {
  type: "symbol",
  actor: "server1.conn1.child1/symbol2",
});

stubs.set("SymbolWithLongString", {
  type: "symbol",
  actor: "server1.conn1.child1/symbol1",
  name: {
    type: "longString",
    initial: "aa".repeat(10000),
    length: 20000,
    actor: "server1.conn1.child1/longString58",
  },
});

module.exports = stubs;
