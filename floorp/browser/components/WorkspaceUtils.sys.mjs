/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const EXPORTED_SYMBOLS = ["WorkspaceUtils"];

export const workspacesPreferences = {
    WORKSPACE_TAB_ENABLED_PREF: "floorp.browser.workspace.tab.enabled",
    WORKSPACE_CURRENT_PREF: "floorp.browser.workspace.current",
    WORKSPACE_ALL_PREF: "floorp.browser.workspace.all",
    WORKSPACE_TABS_PREF: "floorp.browser.workspace.tabs.state",
    WORKSPACE_MANAGE_ON_BMS_PREF: "floorp.browser.workspace.manageOnBMS",
    WORKSPACE_SHOW_WORKSPACE_NAME_PREF: "floorp.browser.workspace.showWorkspaceName",
    WORKSPACE_CLOSE_POPUP_AFTER_CLICK_PREF: "floorp.browser.workspace.closePopupAfterClick",
    WORKSPACE_INFO_PREF: "floorp.browser.workspace.info",
    WORKSPACE_BACKUPED_PREF: "floorp.browser.workspace.backuped",
    WORKSPACE_CHANGE_WORKSPACE_WITH_DEFAULT_KEY_PREF: "floorp.browser.workspace.changeWorkspaceWithDefaultKey",
};

export const defaultWorkspaceName = Services.prefs.getStringPref(
    workspacesPreferences.WORKSPACE_ALL_PREF
    ).split(",")[0];

export const CONTAINER_ICONS = new Set([
  "briefcase",
  "cart",
  "circle",
  "dollar",
  "fence",
  "fingerprint",
  "gift",
  "vacation",
  "food",
  "fruit",
  "pet",
  "tree",
  "chill",
]);
