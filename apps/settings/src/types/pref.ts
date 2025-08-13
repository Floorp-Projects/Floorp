/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";
import { zFloorpDesignConfigs } from "../../../../apps/common/scripts/global-types/type.ts";
import { zWorkspacesServicesConfigs } from "../../../../apps/main/core/common/workspaces/utils/type.ts";
import { zPanelSidebarConfig } from "../../../../apps/main/core/common/panel-sidebar/utils/type.ts";

/* Home */
export const zAccountInfo = z.object({
  email: z.string(),
  status: z.string(),
  displayName: z.string(),
  avatarURL: z.string(),
});

export interface HomeData {
  accountName: string | null;
  accountImage: string;
}

export type AccountInfo = z.infer<typeof zAccountInfo>;

/* Tab & Appearance */
const globalConfig = zFloorpDesignConfigs.shape.globalConfigs.shape;
const tabbar = zFloorpDesignConfigs.shape.tabbar.shape;
const tab = zFloorpDesignConfigs.shape.tab.shape;

export const zDesignFormData = z.object({
  // Global
  design: globalConfig.userInterface,
  faviconColor: z.boolean(),

  // Tab Bar
  style: tabbar.tabbarStyle,
  position: tabbar.tabbarPosition,
  // Tab
  tabOpenPosition: tab.tabOpenPosition,
  tabMinHeight: tab.tabMinHeight,
  tabMinWidth: tab.tabMinWidth,
  tabPinTitle: tab.tabPinTitle,
  tabScroll: tab.tabScroll.shape.enabled,
  tabScrollReverse: tab.tabScroll.shape.reverse,
  tabScrollWrap: tab.tabScroll.shape.wrap,
  tabDubleClickToClose: tab.tabDubleClickToClose,
});

export type DesignFormData = z.infer<typeof zDesignFormData>;

/* Workspaces */
export const zWorkspacesFormData = z.object({
  enabled: z.boolean(),
  manageOnBms: zWorkspacesServicesConfigs.shape.manageOnBms,
  showWorkspaceNameOnToolbar:
    zWorkspacesServicesConfigs.shape.showWorkspaceNameOnToolbar,
  closePopupAfterClick: zWorkspacesServicesConfigs.shape.closePopupAfterClick,
});

export type WorkspacesFormData = z.infer<typeof zWorkspacesFormData>;

/* About */
export type ConstantsData = {
  MOZ_APP_VERSION: string;
  MOZ_APP_VERSION_DISPLAY: string;
  MOZ_OFFICIAL_BRANDING: boolean;
};

/* Accounts */
export const zAccountsFormData = z.object({
  accountInfo: zAccountInfo,
  profileDir: z.string(),
  profileName: z.string(),
  asyncNoesViaMozillaAccount: z.boolean(),
});

export type AccountsFormData = z.infer<typeof zAccountsFormData>;

/* Panel Sidebar */
export const zPanelSidebarFormData = z.object({
  enabled: z.boolean(),
  autoUnload: zPanelSidebarConfig.shape.autoUnload,
  position_start: zPanelSidebarConfig.shape.position_start,
  displayed: zPanelSidebarConfig.shape.displayed,
  webExtensionRunningEnabled:
    zPanelSidebarConfig.shape.webExtensionRunningEnabled,
});

export type PanelSidebarFormData = z.infer<typeof zPanelSidebarFormData>;

/* Progressive Web App */
export const zProgressiveWebAppFormData = z.object({
  enabled: z.boolean(),
  showToolbar: z.boolean(),
});

export const zProgressiveWebAppObject = z.record(z.object({
  id: z.string(),
  name: z.string(),
  short_name: z.string().optional(),
  start_url: z.string(),
  icon: z.string(),
  scope: z.string().optional(),
}));

export type TProgressiveWebAppFormData = z.infer<
  typeof zProgressiveWebAppFormData
>;
export type TProgressiveWebAppObject = z.infer<typeof zProgressiveWebAppObject>;

export type InstalledApp = TProgressiveWebAppObject[string];
