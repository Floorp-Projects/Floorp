import { findChildIndex } from "./dom-utils.ts";
import type { PinnedTabController } from "./pinned-tab-controller.ts";
import type {
  FirefoxWindow,
  PrivateBrowsingUtilsType,
  ServicesType,
  TabBrowser,
  XULTab,
} from "./multibar.d.ts";

declare const gBrowser: TabBrowser;
declare const window: FirefoxWindow;
declare const Services: ServicesType;
declare const PrivateBrowsingUtils: PrivateBrowsingUtilsType;
declare const TAB_DROP_TYPE: string;

export class TabDragDropManager {
  private lastKnownIndex: number | null = null;
  private groupToInsertTo: XULElement | null = null;
  private positionInGroup: number | null = null;
  private draggedTabIndex: number | null = null;
  private listenersActive = false;
  private arrowScrollbox: XULElement | null = null;
  private originalGetDropIndex:
    | ((event: DragEvent) => number)
    | undefined
    | null = null;
  private originalGetDropEffectForTabDrag:
    | ((event: DragEvent) => string)
    | undefined
    | null = null;
  private originalOnDragOver: ((event: DragEvent) => void) | undefined | null =
    null;
  private dropEventListener: ((e: Event) => void) | null = null;

  constructor(
    private readonly resolveTabsContainer: () => XULElement | null,
    private readonly pinnedTabs: PinnedTabController,
  ) {}

  install(arrowScrollbox: XULElement): void {
    this.arrowScrollbox = arrowScrollbox;

    const tabContainer = gBrowser.tabContainer;

    // Save original functions
    this.originalGetDropIndex = tabContainer._getDropIndex;
    this.originalGetDropEffectForTabDrag = tabContainer.getDropEffectForTabDrag;
    this.originalOnDragOver = tabContainer.on_dragover;

    tabContainer._getDropIndex = (event: DragEvent): number => {
      const tabToDropAt = this.getTabFromEventTarget(event);
      if (!tabToDropAt) return 0;

      if (!this.arrowScrollbox) return 0;
      const tabPos = findChildIndex(this.arrowScrollbox, tabToDropAt);
      const rect = tabToDropAt.getBoundingClientRect();
      const isLtr = window.getComputedStyle(tabContainer).direction === "ltr";

      if (isLtr) {
        return event.clientX < rect.x + rect.width / 2 ? tabPos : tabPos + 1;
      }
      return event.clientX > rect.x + rect.width / 2 ? tabPos : tabPos + 1;
    };

    tabContainer.addEventListener("dragstart", (event: Event) => {
      const dragEvent = event as DragEvent;
      const tab = this.getTabFromEventTarget(dragEvent);
      if (!tab || !this.arrowScrollbox) return;

      const pinnedTabsCount = this.arrowScrollbox.querySelectorAll(
        ".tabbrowser-tab[newPin]",
      ).length;
      this.draggedTabIndex = findChildIndex(this.arrowScrollbox, tab);

      const firstTab = document?.getElementsByClassName("tabbrowser-tab")[0];
      if (
        (firstTab &&
          tabContainer.arrowScrollbox.clientHeight > firstTab.clientHeight) ||
        pinnedTabsCount > 0
      ) {
        gBrowser.visibleTabs.forEach((t: XULTab) => {
          t.style.setProperty("transform", "");
        });

        if (!this.listenersActive) {
          tabContainer.getDropEffectForTabDrag = (e: DragEvent) =>
            this.orig_getDropEffectForTabDrag(e);
          tabContainer._getDropEffectForTabDrag = (e: DragEvent) =>
            this.orig_getDropEffectForTabDrag(e);
          tabContainer.on_dragover = this.performTabDragOver;
          tabContainer._onDragOver = this.performTabDragOver;
          this.dropEventListener = (e: Event) => {
            this.performTabDropEvent(e as DragEvent);
          };
          tabContainer.addEventListener("drop", this.dropEventListener);
          this.listenersActive = true;
        }
      }
    });
  }

  uninstall(): void {
    if (!this.arrowScrollbox) return;

    const tabContainer = gBrowser.tabContainer;

    // Restore original functions
    if (this.originalGetDropIndex) {
      tabContainer._getDropIndex = this.originalGetDropIndex;
    }
    if (this.originalGetDropEffectForTabDrag) {
      tabContainer.getDropEffectForTabDrag =
        this.originalGetDropEffectForTabDrag;
      tabContainer._getDropEffectForTabDrag =
        this.originalGetDropEffectForTabDrag;
    }
    if (this.originalOnDragOver) {
      tabContainer.on_dragover = this.originalOnDragOver;
      tabContainer._onDragOver = this.originalOnDragOver;
    }

    // Remove drop event listener
    if (this.dropEventListener) {
      tabContainer.removeEventListener("drop", this.dropEventListener);
      this.dropEventListener = null;
    }

    // Reset state
    this.resetState();
    this.listenersActive = false;
    this.arrowScrollbox = null;
  }

  private getTabFromEventTarget(
    event: DragEvent,
    { ignoreTabSides = false } = {},
  ): XULElement | null {
    let target = event.target as Node;
    if (target.nodeType !== Node.ELEMENT_NODE) {
      target = target.parentElement!;
    }

    const tab = (target as Element)?.closest("tab") ||
      (target as Element)?.closest("tab-group");
    const selectedTab = gBrowser.selectedTab;

    if (tab && ignoreTabSides) {
      const { width, height } = tab.getBoundingClientRect();
      const xulTab = tab as XULTab;
      if (
        event.screenX < xulTab.screenX + width * 0.25 ||
        event.screenX > xulTab.screenX + width * 0.75 ||
        ((event.screenY < xulTab.screenY + height * 0.25 ||
          event.screenY > xulTab.screenY + height * 0.75) &&
          gBrowser.tabContainer.verticalMode)
      ) {
        return selectedTab;
      }
    }

    if (!tab) {
      return selectedTab;
    }

    return tab as XULElement;
  }

  private performTabDragOver = (event: DragEvent): void => {
    event.preventDefault();
    event.stopPropagation();

    const tabContainer = gBrowser.tabContainer;
    const indicator =
      tabContainer.getElementsByClassName("tab-drop-indicator")[0];

    const effects = this.orig_getDropEffectForTabDrag(event);
    let tab: XULElement | null = null;
    if (effects === "link") {
      tab = this.getTabFromEventTarget(event, { ignoreTabSides: true });
      if (tab) {
        if (!tabContainer._dragTime) {
          tabContainer._dragTime = Date.now();
        }
        if (
          !tab.hasAttribute("pendingicon") &&
          tabContainer._dragOverDelay &&
          Date.now() >= tabContainer._dragTime + tabContainer._dragOverDelay
        ) {
          tabContainer.selectedItem = tab;
        }
        (indicator as HTMLElement).hidden = true;
        return;
      }
    }

    if (!tab) {
      tab = this.getTabFromEventTarget(event);
    }
    if (!tab) return;

    if (!tabContainer._getDropIndex) return;
    let dropIndex = tabContainer._getDropIndex(event);
    if (dropIndex == null) return;

    const tabs = tabContainer.querySelectorAll("tab");
    if (tab.nodeName === "tab-group") {
      this.groupToInsertTo = tab;
      const groupStart = Array.prototype.indexOf.call(
        tabs,
        tab.querySelector("tab:first-of-type"),
      );
      const groupEnd = Array.prototype.indexOf.call(
        tabs,
        tab.querySelector("tab:last-of-type"),
      ) + 1;
      this.positionInGroup = groupEnd - groupStart;
      dropIndex = groupEnd;
    } else if (tab.parentElement?.nodeName === "tab-group") {
      this.groupToInsertTo = tab.parentElement as XULElement;
      const groupStart = tab.parentElement.querySelector("tab:first-of-type");
      this.positionInGroup = dropIndex -
        Array.prototype.indexOf.call(tabs, groupStart);
    } else {
      this.groupToInsertTo = null;
      this.positionInGroup = null;
    }

    const ltr = window.getComputedStyle(tabContainer).direction === "ltr";
    const rect = tabContainer.arrowScrollbox.getBoundingClientRect();

    this.lastKnownIndex = dropIndex;

    let newMarginX: number;
    let newMarginY: number;
    if (dropIndex === tabs.length) {
      const tabRect = tabs[dropIndex - 1].getBoundingClientRect();
      newMarginX = ltr ? tabRect.right - rect.left : rect.right - tabRect.left;
      newMarginY = tabRect.top + tabRect.height - rect.top - rect.height;
      if (CSS.supports("offset-anchor", "left bottom")) {
        newMarginY += rect.height / 2 - tabRect.height / 2;
      }
    } else if (dropIndex != null || dropIndex !== 0) {
      const tabRect = tabs[dropIndex].getBoundingClientRect();
      newMarginX = ltr ? tabRect.left - rect.left : rect.right - tabRect.right;
      newMarginY = tabRect.top + tabRect.height - rect.top - rect.height;
      if (CSS.supports("offset-anchor", "left bottom")) {
        newMarginY += rect.height / 2 - tabRect.height / 2;
      }
    } else {
      return;
    }

    newMarginX += indicator.clientWidth / 2;
    if (!ltr) newMarginX *= -1;

    const htmlIndicator = indicator as HTMLElement;
    htmlIndicator.hidden = false;
    htmlIndicator.style.setProperty(
      "transform",
      `translate(${Math.round(newMarginX)}px, ${Math.round(newMarginY)}px)`,
    );
    htmlIndicator.style.setProperty(
      "margin-inline-start",
      -indicator.clientWidth + "px",
    );
  };

  private performTabDropEvent = (event: DragEvent): void => {
    const dt = event.dataTransfer;
    if (!dt) return;

    const dropEffect = dt.dropEffect;
    let draggedTab: XULElement | null = null;
    if (dt.mozTypesAt(0)[0] === TAB_DROP_TYPE) {
      draggedTab = dt.mozGetDataAt(TAB_DROP_TYPE, 0);
      if (!draggedTab) {
        return;
      }
    }

    const tabsContainer = this.resolveTabsContainer();
    if (!tabsContainer) return;

    const allTabs = tabsContainer.querySelectorAll("tab");
    if (this.lastKnownIndex !== null && this.lastKnownIndex >= allTabs.length) {
      this.lastKnownIndex = allTabs.length - 1;
    }

    if (
      draggedTab?.nodeName === "label" &&
      draggedTab.parentNode?.parentNode?.parentNode?.nodeName === "tab-group"
    ) {
      const tabGroup = draggedTab.parentNode.parentNode
        .parentNode as XULElement;
      const tabToMoveTo = allTabs[this.lastKnownIndex!];
      if (this.groupToInsertTo && "querySelectorAll" in tabGroup) {
        const tabs = Array.from(tabGroup.querySelectorAll("tab")) as XULTab[];
        this.moveTabsToGroup(tabs);
      } else if (this.lastKnownIndex !== allTabs.length - 1) {
        gBrowser.moveTabBefore(tabGroup, tabToMoveTo);
      } else {
        gBrowser.moveTabAfter(tabGroup, tabToMoveTo);
      }
    } else if (
      draggedTab &&
      dropEffect !== "copy" &&
      (draggedTab as XULTab).container === gBrowser.tabContainer
    ) {
      // deno-lint-ignore prefer-const
      let newIndex = this.lastKnownIndex;
      if (newIndex === null) return;

      const selectedTabs = gBrowser.selectedTabs.filter(
        (t: XULTab | null) => t != null,
      ) as XULTab[];

      const pinnedTabsCount = tabsContainer.querySelectorAll(
        ".tabbrowser-tab[newPin]",
      ).length;

      if (newIndex >= 0 && newIndex < pinnedTabsCount) {
        selectedTabs.forEach((t: XULTab) => {
          if (newIndex! > this.draggedTabIndex!) {
            newIndex!--;
          }

          if (t.pinned) {
            gBrowser.unpinTab(t);
          }

          gBrowser.pinTab(t);
          const pinned = document?.querySelectorAll(
            "#pinned-tabs-container .tabbrowser-tab",
          );
          if (pinned) {
            this.pinnedTabs.migratePinnedTabs(tabsContainer, pinned);
          }
          setTimeout(() => {
            const tab = tabsContainer.querySelector(
              `.tabbrowser-tab[newPin]:nth-child(${
                tabsContainer.querySelectorAll("tab[newPin]").length
              })`,
            );
            const tabToMoveAt = tabsContainer.childNodes[newIndex!];
            const periphery = document?.getElementById(
              "tabbrowser-arrowscrollbox-periphery",
            );
            if (tab) {
              if (tabToMoveAt == null) {
                if (periphery) {
                  tabsContainer.insertBefore(tab, periphery);
                }
              } else {
                tabsContainer.insertBefore(tab, tabToMoveAt);
              }
            }
          }, 10);
        });
      } else if (this.groupToInsertTo) {
        this.moveTabsToGroup(selectedTabs);
      } else {
        const updatedTabs = tabsContainer.querySelectorAll("tab");
        let tabToMoveTo = updatedTabs[newIndex];
        let shouldMoveAfter =
          tabToMoveTo.parentElement?.nodeName === "tab-group";
        if (shouldMoveAfter) {
          tabToMoveTo = updatedTabs[newIndex - 1];
        } else if (newIndex === updatedTabs.length - 1) {
          shouldMoveAfter = true;
        }

        selectedTabs.forEach((t: XULTab) => {
          if (t.hasAttribute("newPin")) {
            t.removeAttribute("newPin");
          }

          if (!shouldMoveAfter) {
            gBrowser.moveTabBefore(t, tabToMoveTo);
          } else {
            gBrowser.moveTabAfter(t, tabToMoveTo);
          }
        });
      }
    }

    this.resetState();
  };

  private orig_getDropEffectForTabDrag(event: DragEvent): string {
    const dt = event.dataTransfer;
    if (!dt) return "none";

    let isMovingTabs = dt.mozItemCount > 0;
    for (let i = 0; i < dt.mozItemCount; i++) {
      const types = dt.mozTypesAt(0);
      if (types[0] !== TAB_DROP_TYPE) {
        isMovingTabs = false;
        break;
      }
    }

    if (isMovingTabs) {
      const sourceNode = dt.mozGetDataAt(TAB_DROP_TYPE, 0);
      if (
        XULElement.isInstance(sourceNode) &&
        sourceNode.localName === "tab" &&
        sourceNode.ownerGlobal &&
        sourceNode.ownerDocument?.documentElement &&
        sourceNode.ownerGlobal.isChromeWindow &&
        sourceNode.ownerDocument.documentElement.getAttribute("windowtype") ===
          "navigator:browser" &&
        (sourceNode as XULTab).container ===
          sourceNode.ownerGlobal.gBrowser.tabContainer
      ) {
        if (
          PrivateBrowsingUtils.isWindowPrivate(window) !==
            PrivateBrowsingUtils.isWindowPrivate(
              sourceNode.ownerGlobal as unknown as FirefoxWindow,
            )
        ) {
          return "none";
        }

        if (
          window.gMultiProcessBrowser !==
            sourceNode.ownerGlobal.gMultiProcessBrowser
        ) {
          return "none";
        }

        if (window.gFissionBrowser !== sourceNode.ownerGlobal.gFissionBrowser) {
          return "none";
        }

        return dt.dropEffect === "copy" ? "copy" : "move";
      }
    }

    if (Services.droppedLinkHandler.canDropLink(event, true)) return "link";

    return "none";
  }

  private moveTabsToGroup(selectedTabs: XULTab[]): void {
    if (!this.groupToInsertTo) return;

    const tabInGroupToMoveTo = this.groupToInsertTo.querySelector(
      `tab:nth-of-type(${this.positionInGroup! + 1})`,
    );
    selectedTabs.forEach((t: XULTab) => {
      if (t.hasAttribute("newPin")) {
        t.removeAttribute("newPin");
      }
      if (this.groupToInsertTo) {
        gBrowser.moveTabToGroup(t, this.groupToInsertTo);

        if (tabInGroupToMoveTo) {
          gBrowser.moveTabBefore(t, tabInGroupToMoveTo as XULElement);
        } else {
          const lastTab = this.groupToInsertTo.querySelector(
            "tab:last-of-type",
          );
          if (lastTab) {
            gBrowser.moveTabAfter(t, lastTab as XULElement);
          }
        }
      }
    });
  }

  private resetState(): void {
    this.lastKnownIndex = null;
    this.groupToInsertTo = null;
    this.positionInGroup = null;
  }
}
