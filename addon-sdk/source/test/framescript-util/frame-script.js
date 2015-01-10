/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { windowToMessageManager, nodeToMessageManager } = require("framescript/util");


const onInit = (context) => {
  context.addMessageListener("framescript-util/window/request", _ => {
    windowToMessageManager(context.content.window).sendAsyncMessage(
      "framescript-util/window/response", {window: true});
  });

  context.addMessageListener("framescript-util/node/request", message => {
    const node = context.content.document.querySelector(message.data);
    nodeToMessageManager(node).sendAsyncMessage(
      "framescript-util/node/response", {node: true});
  });
};
exports.onInit = onInit;
