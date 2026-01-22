// SPDX-License-Identifier: MPL-2.0
// Minimal ambient types for Gecko/XUL globals used in Floorp chrome code.
// These are intentionally lightweight and only include members we actually use.

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
    },
  ): XULElement;
  removeTab(tab: XULElement): void;
  showTab(tab: XULElement): void;
  hideTab(tab: XULElement): void;
  getBrowserForTab(tab: XULElement): {
    currentURI?: { spec: string };
    loadURI: (url: string, options?: { triggeringPrincipal?: unknown }) => void;
  };
  pinTab(tab: XULElement): void;
  tabGroups: Array<{ tabs: XULElement[]; style: { display: string } }>;
  tabContainer: EventTarget;
}

interface TabContextMenu {
  contextTab: XULElement & { multiselected?: boolean };
}

interface PanelUI {
  showSubView(id: string, anchor?: Element | null): Promise<void>;
}

declare global {
  // Gecko globals
  var gBrowser: GBrowser;
  var TabContextMenu: TabContextMenu;
  var PanelUI: PanelUI;

  // Ensure globalThis is augmented for property access like globalThis.gBrowser
  namespace globalThis {
    // Using 'var'/'const' mirrors runtime globals exposed by Gecko
    // and allows property access off globalThis without index errors.
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    var gBrowser: GBrowser;
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    var TabContextMenu: TabContextMenu;
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    var PanelUI: PanelUI;
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
  }

  // Add custom XUL events we listen to
  interface GlobalEventHandlersEventMap {
    TabOpen: Event;
    TabClose: Event;
  }
}

export {};
