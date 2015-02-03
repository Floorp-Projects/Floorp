/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {loadModule} = require("framescript/manager");
const {withTab, receiveMessage} = require("./util");
const {getBrowserForTab} = require("sdk/tabs/utils");

exports["test windowToMessageManager"] = withTab(function*(assert, tab) {
  const {messageManager} = getBrowserForTab(tab);

  loadModule(messageManager,
             require.resolve("./framescript-util/frame-script"),
             true,
             "onInit");

  messageManager.sendAsyncMessage("framescript-util/window/request");

  const response = yield receiveMessage(messageManager,
                                        "framescript-util/window/response");

  assert.deepEqual(response.data, {window: true},
                   "got response");
}, "data:text/html,<h1>Window to Message Manager</h1>");


exports["test nodeToMessageManager"] = withTab(function*(assert, tab) {
  const {messageManager} = getBrowserForTab(tab);

  loadModule(messageManager,
             require.resolve("./framescript-util/frame-script"),
             true,
             "onInit");

  messageManager.sendAsyncMessage("framescript-util/node/request", "h1");

  const response = yield receiveMessage(messageManager,
                                        "framescript-util/node/response");

  assert.deepEqual(response.data, {node: true},
                   "got response");
}, "data:text/html,<h1>Node to Message Manager</h1>");

require("sdk/test").run(exports);
