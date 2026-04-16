// SPDX-License-Identifier: MPL-2.0
// Minimal ambient types for Gecko/XUL globals used in Floorp chrome code.
// These are intentionally lightweight and only include members we actually use.
//
// IMPORTANT: This file has NO `export {}` — it is a SCRIPT file (not a module).
// All declarations at the top level are automatically global.
// This is required so that `declare var document: Document` can override
// Gecko's `Document | null` (TypeScript uses the first `declare var` it encounters,
// so this file must be listed BEFORE `jsr:@types/gecko` in deno.json `compilerOptions.types`).

// Override Gecko's `Document | null` — in Firefox chrome context, document is always available.
// deno-lint-ignore-file no-var
declare var document: Document;

interface GBrowser {
  tabs: XULElement[];
  visibleTabs: XULElement[];
  selectedTab: XULElement;
  selectedTabs: XULElement[];
  addTab(
    url: string,
    options?: {
      skipAnimation?: boolean;
      inBackground?: boolean;
      userContextId?: number;
      triggeringPrincipal?: unknown;
      pinned?: boolean;
      index?: number;
    },
  ): XULElement;
  addTrustedTab(
    url: string,
    options?: {
      skipAnimation?: boolean;
      inBackground?: boolean;
      userContextId?: number;
      triggeringPrincipal?: unknown;
      noInitialLabel?: boolean;
      nextTo?: XULElement;
      relatedToCurrent?: boolean;
    },
  ): XULElement;
  removeTab(tab: XULElement): void;
  showTab(tab: XULElement): void;
  hideTab(tab: XULElement): void;
  getBrowserForTab(tab: XULElement): {
    currentURI?: { spec: string; scheme?: string; host?: string };
    contentPrincipal?: unknown;
    loadURI: (
      url: string,
      options?: { triggeringPrincipal?: unknown; loadFlags?: number },
    ) => void;
  };
  pinTab(tab: XULElement): void;
  tabGroups: Array<{ tabs: XULElement[]; style: { display: string } }>;
  tabContainer: EventTarget;
  selectedBrowser: {
    currentURI?: nsIURI;
    contentPrincipal?: unknown;
  };
  currentURI?: { spec: string };
  loadURI?(url: string, options?: Record<string, unknown>): void;
  addTabsProgressListener(
    listener: Pick<nsIWebProgressListener, "onLocationChange">,
  ): void;
  removeTabsProgressListener(
    listener: Pick<nsIWebProgressListener, "onLocationChange">,
  ): void;
}

interface TabContextMenu {
  contextTab: XULElement & { multiselected?: boolean };
}

interface PanelUI {
  showSubView(id: string, anchor?: Element | null): Promise<void>;
}

interface GFloorpTabColor {
  setEnable(enabled: boolean): void;
}

interface GFloorpStatusBar {
  setShow: (value: boolean) => void;
}

interface GFloorp {
  tabColor?: GFloorpTabColor;
  statusBar?: GFloorpStatusBar;
  [key: string]: unknown;
}

interface CustomizableUI {
  TYPE_TOOLBAR: "toolbar";
  AREA_NAVBAR: "nav-bar";
  AREA_BOOKMARKS: "PersonalToolbar";
  AREA_TABSTRIP: "TabsToolbar";
  AREA_MENUBAR: "toolbar-menubar";
  registerArea(
    name: string,
    config: { type: string; defaultPlacements: string[] },
  ): void;
  unregisterArea(name: string, arg2?: boolean): void;
  registerToolbarNode(node: Element): void;
}

/** Known CustomizableUI area identifiers. Extensible via `string & {}` for custom areas. */
type TCustomizableUIArea =
  | "nav-bar"
  | "PersonalToolbar"
  | "TabsToolbar"
  | "toolbar-menubar"
  | (string & Record<PropertyKey, never>);

// Gecko globals
declare var gBrowser: GBrowser;
declare var TabContextMenu: TabContextMenu;
declare var PanelUI: PanelUI;
declare var gFloorp: GFloorp;
declare var CustomizableUI: CustomizableUI;

// user_pref — Firefox preference setter used in user.js files
declare function user_pref(
  name: string,
  value: string | number | boolean,
): void;

// Ensure globalThis is augmented for property access like globalThis.gBrowser
// oxlint-disable-next-line no-shadow-restricted-names
declare namespace globalThis {
  // Using 'var'/'const' mirrors runtime globals exposed by Gecko
  // and allows property access off globalThis without index errors.
  var gBrowser: GBrowser;
  var TabContextMenu: TabContextMenu;
  var PanelUI: PanelUI;
  var gFloorp: GFloorp;
  var CustomizableUI: CustomizableUI;

  // Gecko chrome globals used in status reporting and context menus
  var StatusPanel: { _label: string };
  var XULBrowserWindow: {
    statusTextField: { label: string };
  };
  var closeMenus: ((element: Element) => void) | undefined;
  var gContextMenu: { linkURL: string | undefined } | undefined;

  // Gecko session and download globals
  var SessionStore: {
    getCustomTabValue(tab: XULElement, key: string): string;
    setCustomTabValue(tab: XULElement, key: string, value: string): void;
    getLazyTabValue(tab: XULElement, key: string): string;
    getSessionHistory(tab: XULElement): unknown;
    getTabState(tab: XULElement): string;
    undoCloseTab(aIndex?: number): XULElement;
    undoCloseWindow?(aIndex?: number): unknown;
    setWindowState(window: Window, state: string, overwrite?: boolean): void;
    getWindowState(window: Window): string;
    getBrowserState(): string;
    setBrowserState(state: string): void;
    getClosedTabCount(window?: Window): number;
    getClosedTabData(window?: Window): unknown[];
    promiseInitialized: Promise<void>;
    persistTabAttribute(attrName: string): void;
  };
  var DownloadsPanel: {
    show(): Promise<void>;
    richListBox?: Element;
    panel?: Element;
    showPanel?(): void;
    hidePanel?(): void;
    initialize?(): void;
    _initialized?: boolean;
    onDownloadAdded?(download: unknown): void;
    onDownloadAdded_hook?(download: unknown): void;
    contextMenu?: unknown;
  };
  var DownloadsView: {
    richlistbox: Element;
    contextMenu?: unknown;
    onDownloadAdded?(download: unknown): void;
    onDownloadAdded_hook?(download: unknown): void;
  };
  var SidebarController: {
    currentID: string;
    sidebars?: unknown;
    reversePosition?: boolean;
  };

  // Floorp-specific chrome globals
  var gFloorpPageAction: Record<string, unknown>;
  var gFloorpPrivateContainer: unknown;
  var gFloorpPanelSidebarCurrentPanel: unknown;
  var gFloorpPanelSidebar: unknown;
  var floorpWebPanelWindow: unknown;
  var floorpSsbWindow: unknown;
  var floorpBmsUserAgent: unknown;
  var gMiddleClickNewTabUsesPasteboard: unknown;

  // Gecko utility globals
  var openUILinkIn: (
    url: string | nsIURI,
    where: string,
    params?: Record<string, unknown>,
  ) => void;
  var openTrustedLinkIn: (
    url: string | nsIURI,
    where: string,
    params?: Record<string, unknown>,
  ) => void;
  var readFromClipboard: () => string;
  var openPreferences: (pane?: string) => void;
  var ZoomManager: { zoom: number };
  var bmsLoadedURI: string;
  var BROWSER_NEW_TAB_URL: string;
  var BrowserAddonUI: { openAddonsMgr(url: string): void };
  var BrowserCommands: { openTab(options?: Record<string, unknown>): void };
  var BrowserUtils: {
    whereToOpenLink(
      event: Event,
      allowBlank?: boolean,
      ignoreAlt?: boolean,
    ): string;
  };
  var E10SUtils: {
    EXTENSION_REMOTE_TYPE: string;
    deserializePrincipal(principal: unknown): unknown;
    getRemoteTypeForURI(uri: string, ...args: unknown[]): string;
    predictOriginAttributes(
      options: Record<string, unknown>,
    ): Record<string, unknown>;
  };
  var UrlbarUtils: { stripUnsafeProtocolOnPaste(text: string): string };
  var E: unknown;
  var noraAAA: unknown;
}

// Add custom XUL events we listen to
interface GlobalEventHandlersEventMap {
  TabOpen: Event;
  TabClose: Event;
}
