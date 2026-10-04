import type { SettingFieldDefinition } from "./types.ts";

// Search destinations use the existing input IDs. Keep these in sync with field labels.
export const SETTING_FIELDS: SettingFieldDefinition[] = [
  {
    "id": "favicon-color",
    "route": "/features/design",
    "titleKey": "design.useFaviconColorToBackgroundOfNavigationBar",
  },
  {
    "id": "scroll-tab",
    "route": "/features/design",
    "titleKey": "design.tab.scrollTab",
  },
  {
    "id": "reverse-scroll",
    "route": "/features/design",
    "titleKey": "design.tab.reverseScroll",
  },
  {
    "id": "scroll-wrap",
    "route": "/features/design",
    "titleKey": "design.tab.scrollWrap",
  },
  {
    "id": "pin-title",
    "route": "/features/design",
    "titleKey": "design.tab.pinTitle",
  },
  {
    "id": "split-view-dnd-create",
    "route": "/features/design",
    "titleKey": "design.tab.tabDragToSplitCreate",
  },
  {
    "id": "double-click-close",
    "route": "/features/design",
    "titleKey": "design.tab.doubleClickToClose",
  },
  {
    "id": "tab-min-width",
    "route": "/features/design",
    "titleKey": "design.tab.minWidth",
  },
  {
    "id": "tab-min-height",
    "route": "/features/design",
    "titleKey": "design.tab.minHeight",
  },
  {
    "id": "search-bar-top",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.navbar.searchBarTop",
  },
  {
    "id": "disable-fullscreen-notification",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.display.disableFullscreenNotification",
  },
  {
    "id": "delete-browser-border",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.display.deleteBrowserBorder",
  },
  {
    "id": "disable-qr-code-button",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.display.disableQRCodeButton",
  },
  {
    "id": "disable-floorp-start",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.newtab.disableFloorpStart",
  },
  {
    "id": "optimize-for-tree-style-tab",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.special.optimizeForTreeStyleTab",
  },
  {
    "id": "hide-forward-backward-button",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.special.hideForwardBackwardButton",
  },
  {
    "id": "stg-like-workspaces",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.special.stgLikeWorkspaces",
  },
  {
    "id": "bookmark-bar-focus-expand",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.bookmarkBar.focusExpand",
  },
  {
    "id": "multirow-tab-newtab-inside",
    "route": "/features/design",
    "titleKey": "design.uiCustomization.multirowTab.newtabInsideEnabled",
  },
  {
    "id": "tab-sleep-exclusion-enabled",
    "route": "/features/design",
    "titleKey": "design.tabSleepExclusion.enable",
  },
  {
    "id": "open-new-window-behavior",
    "route": "/features/design",
    "titleKey": "design.tabWindowBehavior.openLinks",
  },
  {
    "id": "taskbar-tab-previews",
    "route": "/features/design",
    "titleKey": "design.tabWindowBehavior.taskbarPreviews",
  },
  {
    "id": "enable-tab-stacks",
    "route": "/features/design",
    "titleKey": "design.tabStacks.enableTabStacks",
  },
  {
    "id": "enable-panel",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.enablePanelSidebar",
  },
  {
    "id": "auto-unload",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.autoUnloadOnClose",
  },
  {
    "id": "global-width",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.globalWidth",
  },
  {
    "id": "panel-type",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.panelType",
  },
  {
    "id": "panel-url",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.staticPanel",
  },
  {
    "id": "panel-extensionId",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.extensionPanel",
  },
  {
    "id": "panel-icon",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.icon",
  },
  {
    "id": "panel-width",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.width",
  },
  {
    "id": "panel-zoomLevel",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.zoomLevel",
  },
  {
    "id": "panel-userContextId",
    "route": "/features/sidebar",
    "titleKey": "panelSidebar.container",
  },
  {
    "id": "enable-workspaces",
    "route": "/features/workspaces",
    "titleKey": "workspaces.enableWorkspaces",
  },
  {
    "id": "close-popup",
    "route": "/features/workspaces",
    "titleKey": "workspaces.closePopupWhenSelectingWorkspace",
    "descriptionKey": "workspaces.closePopupWhenSelectingWorkspaceDescription",
  },
  {
    "id": "show-name",
    "route": "/features/workspaces",
    "titleKey": "workspaces.showWorkspaceNameOnToolbar",
  },
  {
    "id": "exit-on-last-tab-close",
    "route": "/features/workspaces",
    "titleKey": "workspaces.exitOnLastTabClose",
  },
  {
    "id": "manage-bms",
    "route": "/features/workspaces",
    "titleKey": "workspaces.manageOnBms",
    "descriptionKey": "workspaces.manageOnBmsDescription",
  },
  {
    "id": "enable-shortcuts",
    "route": "/features/shortcuts",
    "titleKey": "keyboardShortcut.enable",
  },
  {
    "id": "enable-pwa",
    "route": "/features/webapps",
    "titleKey": "progressiveWebApp.enablePwa",
  },
  {
    "id": "show-toolbar",
    "route": "/features/webapps",
    "titleKey": "progressiveWebApp.showToolbar",
  },
  {
    "id": "idle-memory-reclaim-enabled",
    "route": "/features/performance",
    "titleKey": "performance.idleReclaim.enable",
  },
];
