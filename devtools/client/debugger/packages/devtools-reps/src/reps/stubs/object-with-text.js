/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("ShadowRule", {
  type: "object",
  class: "CSSStyleRule",
  actor: "server1.conn3.obj273",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "ObjectWithText",
    text: ".Shadow",
  },
});

stubs.set("CSSMediaRule", {
  type: "object",
  actor: "server2.conn8.child17/obj30",
  class: "CSSMediaRule",
  extensible: true,
  frozen: false,
  sealed: false,
  ownPropertyLength: 0,
  preview: {
    kind: "ObjectWithText",
    text: "(min-height: 680px), screen and (orientation: portrait)",
  },
});

module.exports = stubs;
