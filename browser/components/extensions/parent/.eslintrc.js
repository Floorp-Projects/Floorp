/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  extends: "../../../../toolkit/components/extensions/parent/.eslintrc.js",

  globals: {
    Tab: true,
    TabContext: true,
    Window: true,
    actionContextMenu: true,
    browserActionFor: true,
    clickModifiersFromEvent: true,
    getContainerForCookieStoreId: true,
    getInspectedWindowFront: true,
    getTargetTabIdForToolbox: true,
    getToolboxEvalOptions: true,
    isContainerCookieStoreId: true,
    isPrivateCookieStoreId: true,
    isValidCookieStoreId: true,
    makeWidgetId: true,
    openOptionsPage: true,
    pageActionFor: true,
    replaceUrlInTab: true,
    searchInitialized: true,
    sidebarActionFor: true,
    tabGetSender: true,
    tabTracker: true,
    waitForTabLoaded: true,
    windowTracker: true,
  },
};
