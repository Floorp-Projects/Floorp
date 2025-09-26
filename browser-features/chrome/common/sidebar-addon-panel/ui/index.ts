/* Re-exports UI components from the new sidebar/ui/components folder.
   This file provides the public UI entrypoint for consumers and keeps a
   stable import surface while internal code is moved. Do not duplicate logic here; re-export only. */

/* Core JS components (preserve original export names) */
export { CPanelSidebar } from "./components/panel-sidebar.tsx";
export { SidebarContextMenuElem } from "./components/sidebar-contextMenu.tsx";
export { PanelSidebarAddModal } from "./components/panel-sidebar-modal.tsx";
export { PanelSidebarFloating } from "./components/floating.tsx";

/* Visual building blocks */
export { BrowserBox } from "./components/browser-box.tsx";
export { FloatingSplitter } from "./components/floating-splitter.tsx";
export { SidebarHeader } from "./components/sidebar-header.tsx";
/* The component file exports `PanelSidebarButton` — re-export it under its original
   name and provide an alias `SidebarPanelButton` for compatibility. */
export { PanelSidebarButton } from "./components/sidebar-panel-button.tsx";
export { PanelSidebarButton as SidebarPanelButton } from "./components/sidebar-panel-button.tsx";
export { SidebarSelectbox } from "./components/sidebar-selectbox.tsx";
export { SidebarSplitter } from "./components/sidebar-splitter.tsx";
/* The sidebar file exports PanelSidebarElem; re-export it once and provide a Sidebar alias. */
export { PanelSidebarElem, PanelSidebarElem as Sidebar } from "./components/sidebar.tsx";

/* Browsers (moved under sidebar/panel/browsers) */
export { ChromeSiteBrowser } from "../panel/browsers/chrome-site-browser.tsx";
export { WebSiteBrowser } from "../panel/browsers/web-site-browser.tsx";
export { ExtensionSiteBrowser } from "../panel/browsers/extension-site-browser.tsx";

/* Export types that UI depends on (now provided by core) */
export * from "../../sidebar/core/utils/type.ts";

/* Export styles as inline-able entries for bundler consumers (now sourced from our own styles directory) */
export { default as style } from "../styles/style.css?inline";