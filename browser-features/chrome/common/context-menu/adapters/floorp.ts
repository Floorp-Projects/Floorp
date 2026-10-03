// SPDX-License-Identifier: MPL-2.0

import type { ContextMenuAdapter } from "../types.ts";

const BROWSER_DOCUMENT = "chrome://browser/content/browser.xhtml";

const webPanelContextMenuAdapter: ContextMenuAdapter = {
  key: "floorp.webpanel",
  label: "Web panels",
  documentURIs: [BROWSER_DOCUMENT],
  popupSelectors: ["#webpanel-context"],
  aliases: [
    {
      key: "floorp.webpanel.unload",
      selectors: ['[id="unloadWebpanelMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.mute",
      selectors: ['[id="muteMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.zoom",
      selectors: ['[id="changeZoomLevelMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.zoom-in",
      selectors: ['[id="zoomInMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.zoom-out",
      selectors: ['[id="zoomOutMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.zoom-reset",
      selectors: ['[id="resetZoomMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.change-user-agent",
      selectors: ['[id="changeUAWebpanelMenu"]'],
      source: "floorp",
    },
    {
      key: "floorp.webpanel.delete",
      selectors: ['[id="deleteWebpanelMenu"]'],
      source: "floorp",
    },
  ],
  readonlySelectors: [],
  profiles: [{ key: "default", label: "Web panels" }],
  getProfileKey: () => "default",
};

const workspacesContextMenuAdapter: ContextMenuAdapter = {
  key: "floorp.workspaces",
  label: "Workspaces",
  documentURIs: [BROWSER_DOCUMENT],
  popupSelectors: ["#workspaces-toolbar-item-context-menu"],
  aliases: [
    {
      key: "floorp.workspaces.move-up",
      selectors: [
        "#workspaces-toolbar-item-context-menu > menuitem:nth-of-type(1)",
      ],
      source: "floorp",
    },
    {
      key: "floorp.workspaces.move-down",
      selectors: [
        "#workspaces-toolbar-item-context-menu > menuitem:nth-of-type(2)",
      ],
      source: "floorp",
    },
    {
      key: "floorp.workspaces.delete",
      selectors: [
        "#workspaces-toolbar-item-context-menu > menuitem:nth-of-type(3)",
      ],
      source: "floorp",
    },
    {
      key: "floorp.workspaces.manage",
      selectors: [
        "#workspaces-toolbar-item-context-menu > menuitem:nth-of-type(4)",
      ],
      source: "floorp",
    },
    {
      key: "floorp.workspaces.archive",
      selectors: [
        "#workspaces-toolbar-item-context-menu > menuitem:nth-of-type(5)",
      ],
      source: "floorp",
    },
  ],
  readonlySelectors: [],
  profiles: [{ key: "default", label: "Workspaces" }],
  getProfileKey: () => "default",
};

const tabStacksContextMenuAdapter: ContextMenuAdapter = {
  key: "floorp.tab-stacks",
  label: "Tab stacks and groups",
  documentURIs: [BROWSER_DOCUMENT],
  popupSelectors: ["#floorp-stack-kind-menu"],
  aliases: [],
  readonlySelectors: [],
  profiles: [{ key: "default", label: "Default" }],
  getProfileKey: () => "default",
};

export const floorpContextMenuAdapters: readonly ContextMenuAdapter[] = [
  webPanelContextMenuAdapter,
  workspacesContextMenuAdapter,
  tabStacksContextMenuAdapter,
];
