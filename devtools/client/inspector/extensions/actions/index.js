/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum(
  [
    // Update the extension sidebar with an object TreeView.
    "EXTENSION_SIDEBAR_OBJECT_TREEVIEW_UPDATE",

    // Update the extension sidebar with an object value grip preview.
    "EXTENSION_SIDEBAR_OBJECT_GRIP_VIEW_UPDATE",

    // Switch the extension sidebar into an extension page container.
    "EXTENSION_SIDEBAR_PAGE_UPDATE",

    // Remove an extension sidebar from the inspector store.
    "EXTENSION_SIDEBAR_REMOVE",
  ],
  module.exports
);
