/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EXTENSION_SIDEBAR_OBJECT_TREEVIEW_UPDATE,
  EXTENSION_SIDEBAR_EXPRESSION_RESULT_VIEW_UPDATE,
  EXTENSION_SIDEBAR_PAGE_UPDATE,
  EXTENSION_SIDEBAR_REMOVE,
} = require("devtools/client/inspector/extensions/actions/index");

const INITIAL_SIDEBAR = {};

const reducers = {
  [EXTENSION_SIDEBAR_OBJECT_TREEVIEW_UPDATE](sidebar, { sidebarId, object }) {
    // Update the sidebar to a "object-treeview" which shows
    // the passed object.
    return Object.assign({}, sidebar, {
      [sidebarId]: {
        viewMode: "object-treeview",
        object,
      },
    });
  },

  [EXTENSION_SIDEBAR_EXPRESSION_RESULT_VIEW_UPDATE](
    sidebar,
    { sidebarId, expressionResult, rootTitle }
  ) {
    // Update the sidebar to a "object-treeview" which shows
    // the passed object.
    return Object.assign({}, sidebar, {
      [sidebarId]: {
        viewMode: "object-value-grip-view",
        expressionResult,
        rootTitle,
      },
    });
  },

  [EXTENSION_SIDEBAR_PAGE_UPDATE](sidebar, { sidebarId, iframeURL }) {
    // Update the sidebar to a "object-treeview" which shows
    // the passed object.
    return Object.assign({}, sidebar, {
      [sidebarId]: {
        viewMode: "extension-page",
        iframeURL,
      },
    });
  },

  [EXTENSION_SIDEBAR_REMOVE](sidebar, { sidebarId }) {
    // Remove the sidebar from the Redux store.
    delete sidebar[sidebarId];
    return Object.assign({}, sidebar);
  },
};

module.exports = function(sidebar = INITIAL_SIDEBAR, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return sidebar;
  }
  return reducer(sidebar, action);
};
