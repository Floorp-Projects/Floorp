/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("StyleSheet", {
  type: "object",
  class: "CSSStyleSheet",
  actor: "server1.conn2.obj1067",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "ObjectWithURL",
    url: "https://example.com/styles.css",
  },
});

module.exports = stubs;
