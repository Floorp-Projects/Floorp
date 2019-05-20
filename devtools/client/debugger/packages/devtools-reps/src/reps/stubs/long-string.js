/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const multilineFullText = `a\n${Array(20000)
  .fill("a")
  .join("")}`;
const fullTextLength = multilineFullText.length;
const initialText = multilineFullText.substring(0, 10000);

const stubs = new Map();

stubs.set("testMultiline", {
  type: "longString",
  initial: initialText,
  length: fullTextLength,
  actor: "server1.conn1.child1/longString58",
});

stubs.set("testUnloadedFullText", {
  type: "longString",
  initial: Array(10000)
    .fill("a")
    .join(""),
  length: 20000,
  actor: "server1.conn1.child1/longString58",
});

stubs.set("testLoadedFullText", {
  type: "longString",
  fullText: multilineFullText,
  initial: initialText,
  length: fullTextLength,
  actor: "server1.conn1.child1/longString58",
});

module.exports = stubs;
