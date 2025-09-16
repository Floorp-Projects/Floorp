/* Re-export UI components for the addon-style sidebar panel.
   This redirects to the relocated UI implementation under sidebar/ui so
   consumers can switch to sidebar-addon-panel without import churn. */

export { PanelSidebarElem } from "../sidebar/ui/components/panel-sidebar.tsx";
export { Sidebar } from "../sidebar/ui/components/sidebar.tsx";
export { SidebarHeader } from "../sidebar/ui/components/sidebar-header.tsx";
export { SidebarSelectbox } from "../sidebar/ui/components/sidebar-selectbox.tsx";
export { PanelSidebarButton, SidebarPanelButton } from "../sidebar/ui/components/sidebar-panel-button.tsx";
export { SidebarSplitter } from "../sidebar/ui/components/sidebar-splitter.tsx";
export { PanelSidebarAddModal } from "../sidebar/ui/components/panel-sidebar-modal.tsx";
export { SidebarContextMenuElem } from "../sidebar/ui/components/sidebar-contextMenu.tsx";
export { PanelSidebarFloating } from "../sidebar/ui/components/floating.tsx";
export { FloatingSplitter } from "../sidebar/ui/components/floating-splitter.tsx";
export { BrowserBox } from "../sidebar/ui/components/browser-box.tsx";

/* Styles (keep referencing original CSS until style files are reorganized) */
export { default as style } from "../panel-sidebar/components/style.css?inline";