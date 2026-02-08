/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";

/* Home */
export const zAccountInfo = t.type({
  email: t.string,
  status: t.string,
  displayName: t.string,
  avatarURL: t.string,
});

export interface HomeData {
  accountName: string | null;
  accountImage: string;
}

export type AccountInfo = t.TypeOf<typeof zAccountInfo>;

/* Tab & Appearance */
// Note: For io-ts, we need to access the codec's props directly
// This is a simplified version - full implementation would need proper type extraction
export const zDesignFormData = t.type({
  // Global
  design: t.union([
    t.literal("fluerial"),
    t.literal("lepton"),
    t.literal("photon"),
    t.literal("protonfix"),
    t.literal("proton"),
  ]),
  faviconColor: t.boolean,

  // Tab Bar
  style: t.union([
    t.literal("horizontal"),
    t.literal("vertical"),
    t.literal("multirow"),
  ]),
  position: t.union([
    t.literal("hide-horizontal-tabbar"),
    t.literal("optimise-to-vertical-tabbar"),
    t.literal("bottom-of-navigation-toolbar"),
    t.literal("bottom-of-window"),
    t.literal("default"),
  ]),
  maxRow: t.number,
  maxRowEnabled: t.boolean,

  // Tab
  tabOpenPosition: t.number,
  tabMinHeight: t.number,
  tabMinWidth: t.number,
  tabPinTitle: t.boolean,
  tabScroll: t.boolean,
  tabScrollReverse: t.boolean,
  tabScrollWrap: t.boolean,
  tabDubleClickToClose: t.boolean,

  // UI customization
  navbarPosition: t.union([t.literal("top"), t.literal("bottom")]),
  searchBarTop: t.boolean,
  disableFullscreenNotification: t.boolean,
  deleteBrowserBorder: t.boolean,
  optimizeForTreeStyleTab: t.boolean,
  hideForwardBackwardButton: t.boolean,
  stgLikeWorkspaces: t.boolean,
  multirowTabNewtabInside: t.boolean,
  disableFloorpStart: t.boolean,
  bookmarkBarFocusExpand: t.boolean,
  bookmarkBarPosition: t.union([t.literal("top"), t.literal("bottom")]),
  disableQRCodeButton: t.boolean,
});

export type DesignFormData = t.TypeOf<typeof zDesignFormData>;

/* Workspaces */
export const zWorkspacesFormData = t.type({
  enabled: t.boolean,
  manageOnBms: t.boolean,
  showWorkspaceNameOnToolbar: t.boolean,
  closePopupAfterClick: t.boolean,
  exitOnLastTabClose: t.boolean,
});

export type WorkspacesFormData = t.TypeOf<typeof zWorkspacesFormData>;

/* About */
export type ConstantsData = {
  MOZ_APP_VERSION: string;
  MOZ_APP_VERSION_DISPLAY: string;
  MOZ_OFFICIAL_BRANDING: boolean;
};

/* Accounts */
export const zAccountsFormData = t.type({
  accountInfo: zAccountInfo,
  accountImage: t.string,
  profileDir: t.string,
  profileName: t.string,
  asyncNoesViaMozillaAccount: t.boolean,
});

export type AccountsFormData = t.TypeOf<typeof zAccountsFormData>;

/* Panel Sidebar */
export const zPanelSidebarFormData = t.type({
  enabled: t.boolean,
  autoUnload: t.boolean,
  position_start: t.boolean,
  displayed: t.boolean,
  webExtensionRunningEnabled: t.boolean,
  globalWidth: t.number,
});

export type PanelSidebarFormData = t.TypeOf<typeof zPanelSidebarFormData>;

/* Progressive Web App */
export const zProgressiveWebAppFormData = t.type({
  enabled: t.boolean,
  showToolbar: t.boolean,
});

export const zProgressiveWebAppObject = t.record(
  t.string,
  t.intersection([
    t.type({
      id: t.string,
      name: t.string,
      start_url: t.string,
      icon: t.string,
    }),
    t.partial({
      short_name: t.string,
      scope: t.string,
    }),
  ]),
);

export type TProgressiveWebAppFormData = t.TypeOf<
  typeof zProgressiveWebAppFormData
>;
export type TProgressiveWebAppObject = t.TypeOf<
  typeof zProgressiveWebAppObject
>;

export type InstalledApp = TProgressiveWebAppObject[string];

/* Mouse Gesture */
export const zGestureDirection = t.union([
  t.literal("up"),
  t.literal("down"),
  t.literal("left"),
  t.literal("right"),
  t.literal("upRight"),
  t.literal("upLeft"),
  t.literal("downRight"),
  t.literal("downLeft"),
]);

export const zGestureAction = t.type({
  pattern: t.array(zGestureDirection),
  action: t.string,
});

export const zMouseGestureContextMenu = t.type({
  minDistance: t.number,
  preventionTimeout: t.number,
});

export const zMouseGestureConfig = t.type({
  enabled: t.boolean,
  rockerGesturesEnabled: t.boolean,
  wheelGesturesEnabled: t.boolean,
  sensitivity: t.number,
  showTrail: t.boolean,
  showLabel: t.boolean,
  trailColor: t.string,
  trailWidth: t.number,
  contextMenu: zMouseGestureContextMenu,
  actions: t.array(zGestureAction),
});

export type GestureAction = t.TypeOf<typeof zGestureAction>;
export type MouseGestureConfig = t.TypeOf<typeof zMouseGestureConfig>;
export type GestureDirection = t.TypeOf<typeof zGestureDirection>;

export const zMouseGestureFormData = zMouseGestureConfig;
export type MouseGestureFormData = t.TypeOf<typeof zMouseGestureFormData>;

/* Keyboard Shortcut */
export const zShortcutModifiers = t.type({
  alt: t.boolean,
  ctrl: t.boolean,
  meta: t.boolean,
  shift: t.boolean,
});

export const zShortcutConfig = t.type({
  modifiers: zShortcutModifiers,
  key: t.string,
  action: t.string,
});

export const zKeyboardShortcutConfig = t.type({
  enabled: t.boolean,
  shortcuts: t.record(t.string, zShortcutConfig),
});

export type ShortcutModifiers = t.TypeOf<typeof zShortcutModifiers>;
export type ShortcutConfig = t.TypeOf<typeof zShortcutConfig>;
export type KeyboardShortcutConfig = t.TypeOf<typeof zKeyboardShortcutConfig>;

export const zKeyboardShortcutFormData = zKeyboardShortcutConfig;
export type KeyboardShortcutFormData = t.TypeOf<
  typeof zKeyboardShortcutFormData
>;
