/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("RegExp", {
  type: "object",
  class: "RegExp",
  actor: "server1.conn22.obj39",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 1,
  displayString: "/ab+c/i",
});

module.exports = stubs;
