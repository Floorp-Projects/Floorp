/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};


const { Ci } = require("chrome");

const windowToMessageManager = window =>
  window.
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIDocShell).
    sameTypeRootTreeItem.
    QueryInterface(Ci.nsIDocShell).
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIContentFrameMessageManager);
exports.windowToMessageManager = windowToMessageManager;

const nodeToMessageManager = node =>
  windowToMessageManager(node.ownerDocument.defaultView);
exports.nodeToMessageManager = nodeToMessageManager;
