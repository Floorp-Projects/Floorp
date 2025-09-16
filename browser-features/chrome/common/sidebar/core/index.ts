/* Re-exports core data, utils and extension helpers so other packages can
   import stable core APIs while we gradually move implementation files. */

/* Core data and signals */
export {
  panelSidebarData,
  setPanelSidebarData,
  selectedPanelId,
  setSelectedPanelId,
  panelSidebarConfig,
  setPanelSidebarConfig,
  isFloating,
  setIsFloating,
  isFloatingDragging,
  setIsFloatingDragging,
  isPanelSidebarEnabled,
  setIsPanelSidebarEnabled,
} from "./data.ts";

/* Extension helpers */
export * from "./extension-panels.ts";

/* Types & validators */
export * from "./utils/type.ts";

/* Utilities */
export { getFaviconURLForPanel } from "./utils/favicon-getter.ts";
export { getUserContextColor } from "./utils/userContextColor-getter.ts";
export { PanelSidebarStaticNames } from "./utils/panel-sidebar-static-names.ts";