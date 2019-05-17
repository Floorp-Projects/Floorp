/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("ObjectWithUrl", {
  type: "object",
  class: "Location",
  actor: "server1.conn2.obj272",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 15,
  preview: {
    kind: "ObjectWithURL",
    url: "https://www.mozilla.org/en-US/",
  },
});

module.exports = stubs;
