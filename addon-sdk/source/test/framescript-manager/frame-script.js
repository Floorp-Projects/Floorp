/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {onPing} = require("./pong");

exports.onPing = onPing;

exports.onInit = (context) => {
  context.sendAsyncMessage("framescript-manager/ready", {state: "ready"});
  context.addMessageListener("framescript-manager/ping", exports.onPing);
};
