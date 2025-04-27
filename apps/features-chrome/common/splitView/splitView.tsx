/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SplitViewStaticNames } from "./utils/static-names";
import { render } from "@nora/solid-xul";
import type { TSplitViewDatum, Tab, TabEvent } from "./utils/type";
import splitViewStyles from "./style.css?inline";
import {
  currentSplitView,
  setCurrentSplitView,
  setSplitViewData,
  splitViewData,
  setFixedSplitViewData,
  fixedSplitViewData,
} from "./utils/data";

export class CSplitView {

  constructor() {
    this.initializeStyles();
    this.initializePreferences();
    this.setupEventListeners();
    this.checkAllTabHaveSplitViewAttribute();
  }

  private initializeStyles() {
    render(this.StyleElement, document?.head);
  }

  private initializePreferences() {
    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
  }

  private setupEventListeners() {
    window.addEventListener("TabClose", this.handleTabClose.bind(this));
    window.gBrowser.addProgressListener(this.listener);
    this.addMainPopupShowingListener();
  }

  private listener = {
    onLocationChange: () => {
      this.checkAllTabHaveSplitViewAttribute();
      const tab = window.gBrowser.selectedTab as Tab;
      if (tab) {
        this.updateSplitView(tab);
        this.activateTab(tab);
      }
    },
  };

  private activateTab(tab: Tab) {
    tab.linkedBrowser.docShellIsActive = true;
    tab.linkedBrowser.renderLayers = true;
    document?.getElementById(tab.linkedPanel)?.classList.add("deck-selected");
  }

  private StyleElement(): Element {
    return (<style>{splitViewStyles}</style>) as Element;
  }

  get tabBrowserPanel(): XULElement {
    return document?.getElementById("tabbrowser-tabpanels") as XULElement;
  }

  private get getGeneratedUuid(): string {
    return Services.uuid.generateUUID().toString();
  }

  private getTabId(tab: Tab): string {
    if (!tab) {
      throw new Error("Tab is not defined");
    }
    return (
      tab.getAttribute(SplitViewStaticNames.TabAttributionId) ??
      this.setTabId(tab, this.getGeneratedUuid)
    );
  }

  private setTabId(tab: Tab, id: string): string {
    const currentId = tab.getAttribute(SplitViewStaticNames.TabAttributionId);
    if (currentId) {
      return currentId;
    }

    tab.setAttribute(SplitViewStaticNames.TabAttributionId, id);
    return id;
  }

  public getTabById(id: string): Tab {
    return document?.querySelector(
      `[${SplitViewStaticNames.TabAttributionId}="${id}"]`,
    ) as Tab;
  }

  private checkAllTabHaveSplitViewAttribute() {
    for (const tab of window.gBrowser.tabs) {
      if (!tab.getAttribute(SplitViewStaticNames.TabAttributionId)) {
        this.setTabId(tab, this.getGeneratedUuid);
      }
    }
  }

  private handleTabClose = (event: TabEvent) => {
    const tab = event.target;
    const groupIndex = this.findGroupIndexForTab(tab);

    if (groupIndex < 0) {
      return;
    }

    this.removeTabFromGroup(tab, groupIndex, event.forUnsplit);
  };

  private findGroupIndexForTab(tab: Tab): number {
    return splitViewData().findIndex((group) =>
      group.tabIds.includes(this.getTabId(tab)),
    );
  }

  private removeTabFromGroup(
    tab: Tab,
    groupIndex: number,
    forUnsplit: boolean,
  ) {
    const group = splitViewData()[groupIndex];
    const tabIndex = group.tabIds.indexOf(this.getTabId(tab));
    group.tabIds.splice(tabIndex, 1);

    this.resetTabState(tab, forUnsplit);

    if (group.tabIds.length < 2) {
      this.removeGroup(groupIndex);
    } else {
      this.updateSplitView(
        this.getTabById(group.tabIds[group.tabIds.length - 1]),
      );
    }
  }

  private resetTabState(tab: Tab, forUnsplit: boolean) {
    tab.splitView = false;
    tab.linkedBrowser.spliting = false;

    const container = tab.linkedBrowser.closest(
      ".browserSidebarContainer",
    ) as XULElement;

    if (container) {
      this.resetContainerState(container, forUnsplit);
    }
  }

  private resetContainerState(container: XULElement, forUnsplit: boolean) {
    container.removeAttribute("split");
    container.removeAttribute("style");

    if (!forUnsplit) {
      container.style.display = "none";
    } else {
      container.style.flex = "";
      container.style.order = "";
    }
  }

  private removeGroup(groupIndex: number) {
    if (currentSplitView() === groupIndex) {
      this.resetSplitView();
    }
    setSplitViewData((prev) => prev.filter((_, index) => index !== groupIndex));
  }

  private resetSplitView() {
    for (const tab of window.gBrowser.tabs) {
      this.resetTabState(tab, true);
    }

    setCurrentSplitView(-1);
    this.resetTabBrowserPanel();
    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
  }

  private resetTabBrowserPanel() {
    this.tabBrowserPanel.removeAttribute("split-view");
    this.tabBrowserPanel.style.display = "";
    this.tabBrowserPanel.style.flexDirection = "";
  }

  public handleSplitViewPanelRevseOptionClick(reverse: boolean) {
    this.updateSplitView(window.gBrowser.selectedTab, reverse, null);
    this.updateSelectedItemState();
  }

  public handleSplitViewPanelTypeOptionClick(method: "row" | "column") {
    this.updateSplitView(window.gBrowser.selectedTab, null, method);
    this.updateSelectedItemState();
  }

  private addMainPopupShowingListener() {
    const mainPopup = document?.getElementById("mainPopupSet");
    mainPopup?.addEventListener("popupshowing", this.onMainPopupShowing);
  }

  private onMainPopupShowing = () => {
    if (!window.TabContextMenu.contextTab) {
      return;
    }

    const elem = document?.getElementById("context_splittabs") as XULElement;
    if (elem) {
      this.updateContextMenuElement(elem);
    }
  };

  private updateContextMenuElement(elem: XULElement) {
    const isDisabled = this.contextMenuShouldBeDisabled;
    elem.setAttribute("disabled", String(isDisabled));
    const l10nId = isDisabled
      ? "floorp-split-view-open-menu-disabled"
      : "floorp-split-view-open-menu";
    document?.l10n?.setAttributes(elem, l10nId);
  }

  private get contextMenuShouldBeDisabled() {
    const excludeFixedTabDataList = splitViewData().filter(
      (group) => group.fixedMode !== true,
    );

    return (
      window.TabContextMenu.contextTab === window.gBrowser.selectedTab ||
      this.isTabInAnyGroup(
        window.TabContextMenu.contextTab,
        excludeFixedTabDataList,
      ) ||
      this.isTabInAnyGroup(window.gBrowser.selectedTab, excludeFixedTabDataList)
    );
  }

  private isTabInAnyGroup(tab: Tab, groups: TSplitViewDatum[]) {
    return groups.some((group) => group.tabIds.includes(this.getTabId(tab)));
  }

  public contextSplitTabs() {
    if (window.TabContextMenu.contextTab === window.gBrowser.selectedTab) {
      return;
    }
    const tabs = [
      window.TabContextMenu.contextTab,
      window.gBrowser.selectedTab,
    ];
    this.splitTabs(tabs);
  }

  public splitContextFixedTab() {
    this.splitFixedTab(window.TabContextMenu.contextTab);
  }

  public splitFromCsk() {
    const anotherTab = this.getAnotherTab();
    if (anotherTab) {
      this.splitTabs([anotherTab, window.gBrowser.selectedTab]);
    }
  }

  private getAnotherTab() {
    const currentIndex = window.gBrowser.tabs.indexOf(
      window.gBrowser.selectedTab,
    );
    return (
      window.gBrowser.tabs[currentIndex - 1] ||
      window.gBrowser.tabs[currentIndex + 1] ||
      null
    );
  }

  private splitTabs(tabs: Tab[]) {
    if (tabs.length < 2 || tabs.some((tab) => !tab)) {
      return;
    }
    const existingSplitTab = tabs.find((tab) => tab.splitView);
    if (existingSplitTab) {
      const groupIndex = splitViewData().findIndex(
        (group) =>
          group.tabIds.includes(this.getTabId(existingSplitTab)) &&
          group.fixedMode !== true,
      );
      if (groupIndex >= 0) {
        this.updateSplitView(existingSplitTab);
        return;
      }
    }

    this.createNewSplitGroup(tabs);
    window.gBrowser.selectedTab = tabs[0];
    this.updateSplitView(tabs[0]);
  }

  private createNewSplitGroup(tabs: Tab[]) {
    setSplitViewData((prev) => [
      ...prev,
      {
        tabIds: tabs.map((tab) => this.setTabId(tab, this.getGeneratedUuid)),
        reverse: false,
        method: "row",
      },
    ]);
  }

  private splitFixedTab(tab: Tab) {
    setFixedSplitViewData({
      fixedTabId: this.getTabId(tab),
      options: {
        reverse: false,
        method: "row",
      },
    });
    this.updateSplitView(window.gBrowser.selectedTab);
  }

  private tryToRemoveFixedView() {
    const syncIndex = splitViewData().findIndex(
      (group) => group.fixedMode === true,
    );
    if (syncIndex >= 0) {
      this.removeGroup(syncIndex);
    }

    for (const data of splitViewData()) {
      if (data.fixedMode) {
        this.removeGroup(splitViewData().indexOf(data));
      }
    }
  }

  private updateSplitView(
    tab: Tab,
    reverse: boolean | null = null,
    method: "row" | "column" | null = null,
  ) {
    this.tryToRemoveFixedView();

    const splitData = this.findSplitDataForTab(tab);
    if (!splitData) {
      this.handleNoSplitData();
      const data = fixedSplitViewData();
      if (
        data.fixedTabId &&
        this.getTabById(data.fixedTabId) !== window.gBrowser.selectedTab
      ) {
        const newSplitData = {
          ...fixedSplitViewData().options,
          tabIds: [data.fixedTabId, this.getTabId(tab)],
          fixedMode: true,
          reverse: reverse ?? fixedSplitViewData().options.reverse,
          method: method ?? fixedSplitViewData().options.method,
        } as SplitViewData;
        setSplitViewData((prev) => [...prev, newSplitData]);
        this.activateSplitView(newSplitData, tab);
      }

      if (
        this.getTabById(data.fixedTabId ?? "") === window.gBrowser.selectedTab
      ) {
        this.resetSplitView();
      }

      return;
    }

    const newSplitData = this.createNewSplitData(splitData, reverse, method);
    this.updateSplitDataIfNeeded(splitData, newSplitData);

    if (fixedSplitViewData().fixedTabId) {
      this.resetSplitView();
    }

    if (this.shouldDeactivateSplitView(tab)) {
      this.deactivateSplitView();
    }

    this.activateSplitView(newSplitData, tab);
  }

  private findSplitDataForTab(tab: Tab): TSplitViewDatum | undefined {
    return splitViewData().find((group) =>
      group.tabIds.includes(this.getTabId(tab)),
    );
  }

  private handleNoSplitData() {
    if (currentSplitView() >= 0) {
      this.deactivateSplitView();
    }
  }

  private createNewSplitData(
    splitData: TSplitViewDatum,
    reverse: boolean | null,
    method: "row" | "column" | null,
  ): TSplitViewDatum {
    return {
      ...splitData,
      method: method ?? splitData.method ?? "row",
      reverse: reverse ?? splitData.reverse ?? false,
    };
  }

  private updateSplitDataIfNeeded(
    oldSplitData: TSplitViewDatum,
    newSplitData: TSplitViewDatum,
  ) {
    const index = splitViewData().indexOf(oldSplitData);
    if (index >= 0) {
      setSplitViewData((prev) => {
        const newData = [...prev];
        newData[index] = newSplitData;
        return newData;
      });
    }
  }

  private shouldDeactivateSplitView(tab: Tab): boolean {
    return (
      currentSplitView() >= 0 &&
      !splitViewData()[currentSplitView()].tabIds.includes(this.getTabId(tab))
    );
  }

  private deactivateSplitView() {
    for (const tab of window.gBrowser.tabs) {
      const container = tab.linkedBrowser.closest(".browserSidebarContainer");
      this.resetContainerStyle(container);
      container.removeEventListener("click", this.handleTabClick);
    }
    this.resetTabBrowserPanel();
    Services.prefs.setBoolPref("floorp.browser.splitView.working", false);
    this.setTabsDocShellState(
      splitViewData()[currentSplitView()].tabIds.map((id) =>
        this.getTabById(id),
      ),
      false,
    );
    setCurrentSplitView(-1);
  }

  private activateSplitView(splitData: TSplitViewDatum, activeTab: Tab) {
    this.tabBrowserPanel.setAttribute("split-view", "true");
    Services.prefs.setBoolPref("floorp.browser.splitView.working", true);

    setCurrentSplitView(splitViewData().indexOf(splitData));

    this.applyFlexBoxLayout(
      splitData.tabIds.map((id) => this.getTabById(id)),
      activeTab,
      splitData.reverse,
      splitData.method,
    );

    this.setTabsDocShellState(
      splitData.tabIds.map((id) => this.getTabById(id)),
      true,
    );
  }

  private applyFlexBoxLayout(
    tabs: Tab[],
    activeTab: Tab,
    reverse = true,
    method: "row" | "column" = "column",
  ) {
    this.tabBrowserPanel.style.flexDirection = this.getFlexDirection(
      reverse,
      method,
    );
    tabs.forEach((tab, index) => {
      tab.splitView = true;
      const container = tab.linkedBrowser.closest(
        ".browserSidebarContainer",
      ) as XULElement;
      this.styleContainer(container, tab === activeTab, index);
    });
  }

  private getFlexDirection(reverse: boolean, method: "row" | "column"): string {
    return method === "column"
      ? reverse
        ? "column-reverse"
        : "column"
      : reverse
        ? "row-reverse"
        : "row";
  }

  private styleContainer(
    container: XULElement,
    isActive: boolean,
    index: number,
  ) {
    container.removeAttribute("split-active");
    if (isActive) {
      container.setAttribute("split-active", "true");
    }
    container.setAttribute("split-anim", "true");
    container.addEventListener("click", this.handleTabClick);

    container.style.flex = "1";
    container.style.order = String(index);
  }

  private handleTabClick = (event: Event) => {
    const container = event.currentTarget;
    const tab = window.gBrowser.tabs.find(
      (t: Tab) =>
        t.linkedBrowser.closest(".browserSidebarContainer") === container,
    );
    if (tab && tab !== this.getTabById(fixedSplitViewData().fixedTabId ?? "")) {
      window.gBrowser.selectedTab = tab;
    }
  };

  private setTabsDocShellState(tabs: Tab[], active: boolean) {
    tabs.forEach((tab) => {
      tab.linkedBrowser.spliting = active;
      tab.linkedBrowser.docShellIsActive = active;
      const browser = tab.linkedBrowser.closest(
        ".browserSidebarContainer",
      ) as XULElement;

      if (browser) {
        this.setTabDocShellState(browser, active);
      }
    });
  }

  private setTabDocShellState(browser: XULElement, active: boolean) {
    if (active) {
      browser.setAttribute("split", "true");
      const currentStyle = browser.getAttribute("style");
      browser.setAttribute(
        "style",
        `${currentStyle}
        -moz-subtree-hidden-only-visually: 0;
        visibility: visible !important;`,
      );
    } else {
      browser.removeAttribute("split");
      browser.removeAttribute("style");
    }
  }

  private resetContainerStyle(container: XULElement) {
    container.removeAttribute("split-active");
    container.classList.remove("deck-selected");
    container.style.flex = "";
    container.style.order = "";
  }

  public unsplitCurrentView() {
    const currentTab = window.gBrowser.selectedTab;
    let tabs = splitViewData()[currentSplitView()].tabIds.map((id) =>
      this.getTabById(id),
    );

    if (splitViewData()[currentSplitView()].fixedMode) {
      tabs = [
        currentTab,
        this.getTabById(fixedSplitViewData().fixedTabId ?? ""),
      ];

      setFixedSplitViewData({
        fixedTabId: null,
        options: {
          reverse: false,
          method: "row",
        },
      });
    }

    tabs.forEach((tab) => {
      this.handleTabClose({ target: tab, forUnsplit: true } as TabEvent);
    });
    window.gBrowser.selectedTab = currentTab;
  }

  public updateSelectedItemState() {
    const elements = document?.querySelectorAll(".splitView-select-box");
    if (elements) {
      for (const element of elements) {
        element?.classList.remove("selected");
      }
    }

    const currentData = splitViewData()[currentSplitView()];
    const reverse = currentData.reverse;
    const method = currentData.method;

    if (reverse) {
      document
        ?.getElementById("splitView-position-selector-right")
        ?.classList.add("selected");
    } else {
      document
        ?.getElementById("splitView-position-selector-left")
        ?.classList.add("selected");
    }

    if (method === "column") {
      document
        ?.getElementById("splitView-flex-selector-column")
        ?.classList.add("selected");
    } else {
      document
        ?.getElementById("splitView-flex-selector-row")
        ?.classList.add("selected");
    }
  }
}
