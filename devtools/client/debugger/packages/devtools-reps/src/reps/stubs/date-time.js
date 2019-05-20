/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("DateTime", {
  type: "object",
  class: "Date",
  actor: "server1.conn0.child1/obj32",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    timestamp: 1459372644859,
  },
});

stubs.set("InvalidDateTime", {
  type: "object",
  actor: "server1.conn0.child1/obj32",
  class: "Date",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    timestamp: {
      type: "NaN",
    },
  },
});

module.exports = stubs;
