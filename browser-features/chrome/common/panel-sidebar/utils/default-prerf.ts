/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { PanelSidebarConfig, PanelSidebarData } from "./type";

export const defaultEnabled = true;

const defaultConfig: PanelSidebarConfig = {
  globalWidth: 400,
  autoUnload: false,
  position_start: true,
  displayed: true,
  webExtensionRunningEnabled: false,
};

const defaultData: PanelSidebarData = {
  data: [
    {
      id: "default-panel-bookmarks",
      url: "floorp//bookmarks",
      width: 0,
      type: "static",
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    },
    {
      id: "default-panel-history",
      url: "floorp//history",
      width: 0,
      type: "static",
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    },
    {
      id: "default-panel-downloads",
      url: "floorp//downloads",
      width: 0,
      type: "static",
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    },
    {
      id: "default-panel-notes",
      url: "floorp//notes",
      width: 0,
      type: "static",
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    },
    {
      id: "default-panel-translate-google-com",
      url: "https://translate.google.com",
      width: 0,
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
      type: "web",
    },
    {
      id: "default-panel-docs-floorp-app",
      url: "https://docs.floorp.app/docs/features/",
      width: 0,
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
      type: "web",
    },
  ],
};

export const strDefaultConfig = JSON.stringify(defaultConfig);
export const strDefaultData = JSON.stringify(defaultData);
