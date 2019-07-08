/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EXTENSION_SIDEBAR_OBJECT_TREEVIEW_UPDATE,
  EXTENSION_SIDEBAR_OBJECT_GRIP_VIEW_UPDATE,
  EXTENSION_SIDEBAR_PAGE_UPDATE,
  EXTENSION_SIDEBAR_REMOVE,
} = require("./index");

module.exports = {
  /**
   * Update the sidebar with an object treeview.
   */
  updateObjectTreeView(sidebarId, object) {
    return {
      type: EXTENSION_SIDEBAR_OBJECT_TREEVIEW_UPDATE,
      sidebarId,
      object,
    };
  },

  /**
   * Update the sidebar with an object actor preview.
   */
  updateObjectValueGripView(sidebarId, objectValueGrip, rootTitle) {
    return {
      type: EXTENSION_SIDEBAR_OBJECT_GRIP_VIEW_UPDATE,
      sidebarId,
      objectValueGrip,
      rootTitle,
    };
  },

  /**
   * Switch the sidebar into the extension page mode.
   */
  updateExtensionPage(sidebarId, iframeURL) {
    return {
      type: EXTENSION_SIDEBAR_PAGE_UPDATE,
      sidebarId,
      iframeURL,
    };
  },

  /**
   * Remove the extension sidebar from the inspector store.
   */
  removeExtensionSidebar(sidebarId) {
    return {
      type: EXTENSION_SIDEBAR_REMOVE,
      sidebarId,
    };
  },
};
