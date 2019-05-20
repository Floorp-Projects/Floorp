/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Window", {
  type: "object",
  class: "Window",
  actor: "server1.conn3.obj198",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 887,
  preview: {
    kind: "ObjectWithURL",
    url: "about:newtab",
  },
});

module.exports = stubs;
