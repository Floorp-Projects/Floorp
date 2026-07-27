/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import tabStacksStyles from "./styles.css?inline";

export const TAB_STACKS_ROW_ID = "floorp-tab-stacks-row";
export const TAB_STACKS_STYLE_ID = "floorp-tab-stacks-styles";
export const VERTICAL_TABS_PREF = "sidebar.verticalTabs";
export const WORKSPACES_CHANGED_TOPIC = "floorp.workspaces.changed";

export const TAB_STACKS_REBUILD_EVENTS = [
  "TabGroupUpdate",
  "TabGrouped",
  "TabUngrouped",
  "TabShow",
  "TabHide",
  "TabSelect",
  "TabGroupCreate",
  "TabGroupRemoved",
  "TabGroupCollapse",
  "TabGroupExpand",
] as const;

type Observer = (
  subject?: unknown,
  topic?: string,
  data?: string,
) => void;

export interface TabStacksServices {
  prefs: {
    getBoolPref(name: string, fallback?: boolean): boolean;
    addObserver(name: string, observer: Observer): void;
    removeObserver(name: string, observer: Observer): void;
  };
  obs: {
    addObserver(observer: Observer, topic: string): void;
    removeObserver(observer: Observer, topic: string): void;
  };
}

export interface TabStacksLogger {
  debug(message: string): void;
  info(message: string): void;
  warn(message: string): void;
}

export type NativeStackTab = XULElement & {
  label?: string;
  linkedPanel?: string;
  selected?: boolean;
  hidden?: boolean;
  closing?: boolean;
  splitview?: XULElement | null;
};

export type NativeStackGroup = XULElement & {
  id: string;
  label?: string;
  defaultGroupName: string;
  color?: string;
  collapsed?: boolean;
  tabs: NativeStackTab[];
  tabsAndSplitViews?: Array<XULElement>;
};

export interface TabStacksBrowser {
  tabGroups: NativeStackGroup[];
  visibleTabs: NativeStackTab[];
  selectedTab: NativeStackTab;
}

export interface TabStacksControllerOptions {
  document: Document;
  eventTarget: EventTarget;
  services: TabStacksServices;
  getBrowser: () => TabStacksBrowser | null;
  logger?: TabStacksLogger;
}

export interface ActiveGroupSnapshot {
  group: NativeStackGroup;
  groupName: string;
  eligibleTabs: NativeStackTab[];
  selectedTab: NativeStackTab;
}

const noopLogger: TabStacksLogger = {
  debug: () => {},
  info: () => {},
  warn: () => {},
};

function isSplitViewWrapper(value: Element | null | undefined): boolean {
  return value?.tagName?.toLowerCase() === "tab-split-view-wrapper";
}

/**
 * Split View can expose both flattened `group.tabs` and wrapper-preserving
 * `group.tabsAndSplitViews`. A native group containing either representation
 * is intentionally ineligible for this foundation mirror.
 */
export function groupContainsSplitView(
  group: NativeStackGroup,
): boolean {
  if (
    group.tabsAndSplitViews?.some((item) => isSplitViewWrapper(item)) ||
    Array.from(group.children ?? []).some((item) => isSplitViewWrapper(item)) ||
    isSplitViewWrapper(group.closest?.("tab-split-view-wrapper"))
  ) {
    return true;
  }

  return group.tabs.some((tab) => {
    return Boolean(tab.splitview) ||
      isSplitViewWrapper(tab.closest?.("tab-split-view-wrapper")) ||
      tab.hasAttribute?.("floorpSplitViewGroupId") ||
      tab.hasAttribute?.("floorpsplitviewgroupid") ||
      tab.hasAttribute?.("data-floorp-split-tab");
  });
}

function isEligibleVisibleTab(
  tab: NativeStackTab,
  visibleTabs: Set<NativeStackTab>,
): boolean {
  return visibleTabs.has(tab) &&
    tab.isConnected !== false &&
    !tab.hidden &&
    !tab.closing &&
    !tab.hasAttribute?.("hidden");
}

/**
 * Resolve the row entirely from current native state. No membership, label,
 * color, collapse, or selection state is persisted by the feature.
 */
export function getActiveGroupSnapshot(
  browser: TabStacksBrowser | null,
): ActiveGroupSnapshot | null {
  if (!browser?.selectedTab) {
    return null;
  }

  const selectedTab = browser.selectedTab;
  const group = Array.from(browser.tabGroups ?? []).find((candidate) =>
    Array.from(candidate.tabs ?? []).includes(selectedTab)
  );

  if (
    !group ||
    !group.isConnected ||
    group.collapsed ||
    group.hasAttribute?.("collapsed") ||
    groupContainsSplitView(group)
  ) {
    return null;
  }

  const visibleTabs = new Set(browser.visibleTabs ?? []);
  const eligibleTabs = Array.from(group.tabs ?? []).filter((tab) =>
    isEligibleVisibleTab(tab, visibleTabs)
  );

  if (
    eligibleTabs.length < 2 ||
    !eligibleTabs.includes(selectedTab)
  ) {
    return null;
  }

  return {
    group,
    groupName: group.label || group.defaultGroupName,
    eligibleTabs,
    selectedTab,
  };
}

function createHTMLElement<K extends keyof HTMLElementTagNameMap>(
  document: Document,
  name: K,
): HTMLElementTagNameMap[K] {
  return document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    name,
  ) as HTMLElementTagNameMap[K];
}

function getTabLabel(tab: NativeStackTab): string {
  return tab.label ?? tab.getAttribute?.("label") ?? "";
}

function getLinkedPanelId(tab: NativeStackTab): string {
  return tab.linkedPanel ?? tab.getAttribute?.("linkedpanel") ?? "";
}

export class TabStacksController {
  private readonly document: Document;
  private readonly eventTarget: EventTarget;
  private readonly services: TabStacksServices;
  private readonly getBrowser: () => TabStacksBrowser | null;
  private readonly logger: TabStacksLogger;
  private initialized = false;
  private row: HTMLElement | null = null;
  private styleElement: HTMLStyleElement | null = null;
  private domReadyListening = false;
  private readonly proxyTargets = new WeakMap<
    HTMLElement,
    { group: NativeStackGroup; tab: NativeStackTab }
  >();

  constructor(options: TabStacksControllerOptions) {
    this.document = options.document;
    this.eventTarget = options.eventTarget;
    this.services = options.services;
    this.getBrowser = options.getBrowser;
    this.logger = options.logger ?? noopLogger;
  }

  init(): void {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    for (const type of TAB_STACKS_REBUILD_EVENTS) {
      this.eventTarget.addEventListener(type, this.onNativeStateChanged);
    }
    this.eventTarget.addEventListener("unload", this.onUnload);
    this.services.prefs.addObserver(
      VERTICAL_TABS_PREF,
      this.onVerticalTabsChanged,
    );
    this.services.obs.addObserver(
      this.onWorkspaceChanged,
      WORKSPACES_CHANGED_TOPIC,
    );

    if (this.document.readyState === "loading") {
      this.document.addEventListener("DOMContentLoaded", this.onDomReady);
      this.domReadyListening = true;
    }

    this.rebuild();
  }

  rebuild(): void {
    if (!this.initialized) {
      return;
    }

    if (
      this.services.prefs.getBoolPref(VERTICAL_TABS_PREF, false)
    ) {
      this.destroyPresentation();
      return;
    }

    this.ensureStyles();
    const snapshot = getActiveGroupSnapshot(this.getBrowser());
    if (!snapshot) {
      this.destroyRow();
      return;
    }

    const toolbox = this.document.getElementById("navigator-toolbox");
    const navBar = this.document.getElementById("nav-bar");
    if (!toolbox || !navBar || navBar.parentNode !== toolbox) {
      this.destroyRow();
      this.logger.warn(
        "Tab stacks foundation could not find the horizontal toolbox mount point.",
      );
      return;
    }

    const focusedIndex = this.getFocusedProxyIndex();
    this.destroyRow();

    const row = createHTMLElement(this.document, "div");
    row.id = TAB_STACKS_ROW_ID;
    row.className = "floorp-tab-stacks-row";
    row.setAttribute("role", "tablist");
    row.setAttribute("aria-label", snapshot.groupName);
    row.dataset.groupId = snapshot.group.id ?? "";
    row.dataset.groupColor = snapshot.group.color ?? "";

    const nativeColor = snapshot.group.style?.getPropertyValue(
      "--tab-group-color",
    );
    if (nativeColor) {
      row.style.setProperty("--floorp-tab-stacks-color", nativeColor);
    } else if (snapshot.group.color) {
      row.style.setProperty(
        "--floorp-tab-stacks-color",
        `var(--tab-group-color-${snapshot.group.color})`,
      );
    }

    const groupMeta = createHTMLElement(this.document, "div");
    groupMeta.className = "floorp-tab-stacks-group-meta";
    groupMeta.setAttribute("aria-hidden", "true");

    const color = createHTMLElement(this.document, "span");
    color.className = "floorp-tab-stacks-group-color";

    const name = createHTMLElement(this.document, "span");
    name.className = "floorp-tab-stacks-group-name";
    name.textContent = snapshot.groupName;

    const count = createHTMLElement(this.document, "span");
    count.className = "floorp-tab-stacks-group-count";
    count.textContent = String(snapshot.eligibleTabs.length);
    count.setAttribute("aria-hidden", "true");

    groupMeta.append(color, name, count);
    row.appendChild(groupMeta);

    snapshot.eligibleTabs.forEach((tab, index) => {
      const proxy = createHTMLElement(this.document, "button");
      const selected = tab === snapshot.selectedTab;
      proxy.type = "button";
      proxy.className = "floorp-tab-stacks-proxy";
      proxy.dataset.proxyIndex = String(index);
      if (tab.id) {
        proxy.dataset.nativeTabId = tab.id;
      }
      this.proxyTargets.set(proxy, { group: snapshot.group, tab });
      proxy.setAttribute("role", "tab");
      proxy.setAttribute("aria-label", getTabLabel(tab));
      proxy.setAttribute("aria-selected", String(selected));
      proxy.tabIndex = selected ? 0 : -1;

      const linkedPanelId = getLinkedPanelId(tab);
      if (linkedPanelId && this.document.getElementById(linkedPanelId)) {
        proxy.setAttribute("aria-controls", linkedPanelId);
      }

      const label = createHTMLElement(this.document, "span");
      label.className = "floorp-tab-stacks-proxy-label";
      label.setAttribute("aria-hidden", "true");
      label.textContent = getTabLabel(tab);
      proxy.appendChild(label);
      row.appendChild(proxy);
    });

    row.addEventListener("click", this.onRowClick);
    row.addEventListener("keydown", this.onRowKeyDown);
    toolbox.insertBefore(row, navBar);
    this.row = row;

    if (focusedIndex !== null) {
      const proxies = this.getProxyElements();
      const target = proxies[Math.min(focusedIndex, proxies.length - 1)];
      target?.focus();
    }
  }

  destroy(): void {
    if (!this.initialized) {
      this.destroyPresentation();
      return;
    }

    for (const type of TAB_STACKS_REBUILD_EVENTS) {
      this.eventTarget.removeEventListener(type, this.onNativeStateChanged);
    }
    this.eventTarget.removeEventListener("unload", this.onUnload);
    this.services.prefs.removeObserver(
      VERTICAL_TABS_PREF,
      this.onVerticalTabsChanged,
    );
    this.services.obs.removeObserver(
      this.onWorkspaceChanged,
      WORKSPACES_CHANGED_TOPIC,
    );

    if (this.domReadyListening) {
      this.document.removeEventListener("DOMContentLoaded", this.onDomReady);
      this.domReadyListening = false;
    }

    this.destroyPresentation();
    this.initialized = false;
  }

  getRowElement(): HTMLElement | null {
    return this.row;
  }

  getStyleElement(): HTMLStyleElement | null {
    return this.styleElement;
  }

  private readonly onNativeStateChanged = (): void => {
    this.rebuild();
  };

  private readonly onWorkspaceChanged: Observer = (): void => {
    this.rebuild();
  };

  private readonly onVerticalTabsChanged: Observer = (): void => {
    if (this.services.prefs.getBoolPref(VERTICAL_TABS_PREF, false)) {
      this.destroyPresentation();
      return;
    }
    this.rebuild();
  };

  private readonly onDomReady = (): void => {
    this.document.removeEventListener("DOMContentLoaded", this.onDomReady);
    this.domReadyListening = false;
    this.rebuild();
  };

  private readonly onUnload = (): void => {
    this.destroy();
  };

  private readonly onRowClick = (event: Event): void => {
    const target = event.target as Element | null;
    const proxy = target?.closest?.(".floorp-tab-stacks-proxy") as
      | HTMLElement
      | null;
    if (!proxy || !this.row?.contains(proxy)) {
      return;
    }
    this.activateProxy(proxy);
  };

  private readonly onRowKeyDown = (event: Event): void => {
    const keyEvent = event as KeyboardEvent;
    const target = keyEvent.target as Element | null;
    const proxy = target?.closest?.(".floorp-tab-stacks-proxy") as
      | HTMLElement
      | null;
    if (!proxy || !this.row?.contains(proxy)) {
      return;
    }

    const proxies = this.getProxyElements();
    const currentIndex = proxies.indexOf(proxy);
    if (currentIndex < 0) {
      return;
    }

    let focusIndex: number | null = null;
    switch (keyEvent.key) {
      case "ArrowLeft":
        focusIndex = (currentIndex - 1 + proxies.length) % proxies.length;
        break;
      case "ArrowRight":
        focusIndex = (currentIndex + 1) % proxies.length;
        break;
      case "Home":
        focusIndex = 0;
        break;
      case "End":
        focusIndex = proxies.length - 1;
        break;
      case "Enter":
      case " ":
      case "Spacebar":
        keyEvent.preventDefault();
        this.activateProxy(proxy);
        return;
      default:
        return;
    }

    keyEvent.preventDefault();
    proxies[focusIndex]?.focus();
  };

  private activateProxy(proxy: HTMLElement): void {
    const target = this.proxyTargets.get(proxy);
    if (!target) {
      return;
    }

    const browser = this.getBrowser();
    const snapshot = getActiveGroupSnapshot(browser);
    if (
      !browser ||
      !snapshot ||
      snapshot.group !== target.group ||
      !snapshot.eligibleTabs.includes(target.tab)
    ) {
      this.rebuild();
      return;
    }

    // Selecting an existing native tab is the mirror's only product mutation.
    browser.selectedTab = target.tab;
  }

  private getProxyElements(): HTMLElement[] {
    return this.row
      ? Array.from(
        this.row.querySelectorAll<HTMLElement>(".floorp-tab-stacks-proxy"),
      )
      : [];
  }

  private getFocusedProxyIndex(): number | null {
    const active = this.document.activeElement as HTMLElement | null;
    if (!active || !this.row?.contains(active)) {
      return null;
    }
    const proxy = active.closest?.(".floorp-tab-stacks-proxy") as
      | HTMLElement
      | null;
    const index = Number(proxy?.dataset.proxyIndex);
    return Number.isInteger(index) && index >= 0 ? index : null;
  }

  private ensureStyles(): void {
    if (this.styleElement?.isConnected) {
      return;
    }
    const head = this.document.head;
    if (!head) {
      return;
    }

    const style = this.document.createElement("style");
    style.id = TAB_STACKS_STYLE_ID;
    style.textContent = tabStacksStyles;
    head.appendChild(style);
    this.styleElement = style;
  }

  private destroyRow(): void {
    if (!this.row) {
      return;
    }
    this.row.removeEventListener("click", this.onRowClick);
    this.row.removeEventListener("keydown", this.onRowKeyDown);
    this.row.remove();
    this.row = null;
  }

  private destroyPresentation(): void {
    this.destroyRow();
    this.styleElement?.remove();
    this.styleElement = null;
  }
}
