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

export type DropIndicatorTarget = {
  tabIndex: number;
  atEnd: boolean;
};

export function resolveDropIndicatorTarget(
  dropIndex: number,
  tabCount: number,
): DropIndicatorTarget | null {
  if (tabCount <= 0 || dropIndex < 0 || dropIndex > tabCount) {
    return null;
  }
  return dropIndex === tabCount
    ? { tabIndex: tabCount - 1, atEnd: true }
    : { tabIndex: dropIndex, atEnd: false };
}

export function cleanupOwnedDropIndicator(
  indicator: XULElement | null,
): void {
  if (!indicator) return;

  try {
    indicator.hidden = true;
  } catch (error) {
    console.error(
      "[TabDragDropManager] Failed to hide the drop indicator:",
      error,
    );
  }

  try {
    indicator.style.removeProperty("transform");
  } catch (error) {
    console.error(
      "[TabDragDropManager] Failed to clear the drop indicator transform:",
      error,
    );
  }

  try {
    indicator.style.removeProperty("margin-inline-start");
  } catch (error) {
    console.error(
      "[TabDragDropManager] Failed to clear the drop indicator margin:",
      error,
    );
  }
}

export class DropIndicatorOwnership {
  private indicator: XULElement | null = null;

  acquire(indicator: XULElement): XULElement {
    if (this.indicator === indicator) return indicator;

    const previousIndicator = this.take();
    cleanupOwnedDropIndicator(previousIndicator);
    this.indicator = indicator;
    return indicator;
  }

  take(): XULElement | null {
    const indicator = this.indicator;
    this.indicator = null;
    return indicator;
  }
}

export class TabDragDropManager {
  private lastKnownIndex: number | null = null;
  private groupToInsertTo: XULElement | null = null;
  private positionInGroup: number | null = null;
  private draggedTabIndex: number | null = null;
  private listenersActive = false;
  private arrowScrollbox: XULElement | null = null;
  private originalGetDropIndex:
    | ((event: DragEvent) => number)
    | undefined;
  private originalGetDropEffectForTabDrag:
    | ((event: DragEvent) => string)
    | undefined;
  private originalUnderscoreGetDropEffectForTabDrag:
    | ((event: DragEvent) => string)
    | undefined;
  private hadOwnGetDropIndex = false;
  private hadOwnGetDropEffectForTabDrag = false;
  private hadOwnUnderscoreGetDropEffectForTabDrag = false;
  private dropEventListener: ((e: Event) => void) | null = null;
  private dragOverEventListener: ((e: Event) => void) | null = null;
  private dragEndEventListener: ((e: Event) => void) | null = null;
  private dragStartEventListener: ((e: Event) => void) | null = null;
  private readonly dropIndicatorOwnership = new DropIndicatorOwnership();

  constructor(
    private readonly resolveTabsContainer: () => XULElement | null,
    private readonly pinnedTabs: PinnedTabController,
  ) {}

  install(arrowScrollbox: XULElement): void {
    if (this.arrowScrollbox) {
      this.uninstall();
    }
    this.arrowScrollbox = arrowScrollbox;

    const tabContainer = gBrowser.tabContainer;

    // Save original functions. These live on `tabDragAndDrop` in current
    // Firefox (the XBL `on_dragover` property just forwards to it), but older
    // versions exposed them directly on the container — keep both fallbacks.
    this.hadOwnGetDropIndex = Object.hasOwn(tabContainer, "_getDropIndex");
    this.hadOwnGetDropEffectForTabDrag = Object.hasOwn(
      tabContainer,
      "getDropEffectForTabDrag",
    );
    this.hadOwnUnderscoreGetDropEffectForTabDrag = Object.hasOwn(
      tabContainer,
      "_getDropEffectForTabDrag",
    );
    this.originalGetDropIndex = tabContainer._getDropIndex;
    this.originalGetDropEffectForTabDrag = tabContainer.getDropEffectForTabDrag;
    this.originalUnderscoreGetDropEffectForTabDrag =
      tabContainer._getDropEffectForTabDrag;

    // Register the dragover/drop handlers in the CAPTURE phase so they run
    // BEFORE Firefox's native tabDragAndDrop handlers. We can't override the
    // native handlers by assigning to `on_dragover` / `_onDragOver` anymore —
    // current Firefox routes those events directly to `tabDragAndDrop.handle_*`
    // and ignores JS property assignments on XUL elements. Calling
    // `stopPropagation()` inside the capture-phase handler blocks the native
    // handler entirely.
    this.dragOverEventListener = (e: Event) => {
      if (!this.listenersActive) return;
      try {
        this.performTabDragOver(e as DragEvent);
      } catch (error) {
        console.error("[TabDragDropManager] dragover failed:", error);
        this.deactivateDragSession();
      }
    };
    tabContainer.addEventListener("dragover", this.dragOverEventListener, true);

    this.dropEventListener = (e: Event) => {
      if (!this.listenersActive) return;
      const dragEvent = e as DragEvent;
      dragEvent.preventDefault();
      dragEvent.stopPropagation();
      try {
        this.performTabDropEvent(dragEvent);
      } catch (error) {
        console.error("[TabDragDropManager] drop failed:", error);
      } finally {
        this.deactivateDragSession();
      }
    };
    tabContainer.addEventListener("drop", this.dropEventListener, true);

    this.dragStartEventListener = (event: Event) => {
      this.deactivateDragSession();
      const dragEvent = event as DragEvent;
      const tab = this.getTabFromEventTarget(dragEvent);
      if (!tab || !this.arrowScrollbox) {
        return;
      }

      const pinnedTabsCount = this.arrowScrollbox.querySelectorAll(
        ".tabbrowser-tab[newPin]",
      ).length;
      this.draggedTabIndex = findChildIndex(this.arrowScrollbox, tab);

      const firstTab = document?.getElementsByClassName("tabbrowser-tab")[0];
      const isMultiRow = firstTab &&
        tabContainer.arrowScrollbox.clientHeight > firstTab.clientHeight;
      if (isMultiRow || pinnedTabsCount > 0) {
        gBrowser.visibleTabs.forEach((t: XULTab) => {
          t.style.setProperty("transform", "");
        });

        this.activateDragSession();
      }
    };
    tabContainer.addEventListener("dragstart", this.dragStartEventListener);

    this.dragEndEventListener = () => {
      this.deactivateDragSession();
    };
    tabContainer.addEventListener("dragend", this.dragEndEventListener);
  }

  uninstall(): void {
    this.deactivateDragSession();

    if (!this.arrowScrollbox) return;

    const tabContainer = gBrowser.tabContainer;

    // Remove capture-phase listeners
    if (this.dragOverEventListener) {
      tabContainer.removeEventListener(
        "dragover",
        this.dragOverEventListener,
        true,
      );
      this.dragOverEventListener = null;
    }
    if (this.dropEventListener) {
      tabContainer.removeEventListener("drop", this.dropEventListener, true);
      this.dropEventListener = null;
    }
    if (this.dragStartEventListener) {
      tabContainer.removeEventListener(
        "dragstart",
        this.dragStartEventListener,
      );
      this.dragStartEventListener = null;
    }
    if (this.dragEndEventListener) {
      tabContainer.removeEventListener("dragend", this.dragEndEventListener);
      this.dragEndEventListener = null;
    }

    // Reset state
    this.arrowScrollbox = null;
  }

  private activateDragSession(): void {
    const tabContainer = gBrowser.tabContainer;
    tabContainer._getDropIndex = (event: DragEvent): number => {
      const tabToDropAt = this.getTabFromEventTarget(event);
      if (!tabToDropAt || !this.arrowScrollbox) return 0;
      const tabPos = findChildIndex(this.arrowScrollbox, tabToDropAt);
      const rect = tabToDropAt.getBoundingClientRect();
      const isLtr = window.getComputedStyle(tabContainer).direction === "ltr";
      if (isLtr) {
        return event.clientX < rect.x + rect.width / 2 ? tabPos : tabPos + 1;
      }
      return event.clientX > rect.x + rect.width / 2 ? tabPos : tabPos + 1;
    };
    tabContainer.getDropEffectForTabDrag = (event: DragEvent) =>
      this.orig_getDropEffectForTabDrag(event);
    tabContainer._getDropEffectForTabDrag = (event: DragEvent) =>
      this.orig_getDropEffectForTabDrag(event);
    this.listenersActive = true;
  }

  private deactivateDragSession(): void {
    const ownedIndicator = this.dropIndicatorOwnership.take();

    try {
      const tabContainer = gBrowser.tabContainer;
      if (this.listenersActive) {
        this.restoreOverride("_getDropIndex", () => {
          if (this.hadOwnGetDropIndex) {
            tabContainer._getDropIndex = this.originalGetDropIndex;
          } else {
            delete tabContainer._getDropIndex;
          }
        });
        this.restoreOverride("getDropEffectForTabDrag", () => {
          if (this.hadOwnGetDropEffectForTabDrag) {
            tabContainer.getDropEffectForTabDrag =
              this.originalGetDropEffectForTabDrag;
          } else {
            delete tabContainer.getDropEffectForTabDrag;
          }
        });
        this.restoreOverride("_getDropEffectForTabDrag", () => {
          if (this.hadOwnUnderscoreGetDropEffectForTabDrag) {
            tabContainer._getDropEffectForTabDrag =
              this.originalUnderscoreGetDropEffectForTabDrag;
          } else {
            delete tabContainer._getDropEffectForTabDrag;
          }
        });
      }
    } catch (error) {
      console.error(
        "[TabDragDropManager] Failed to access the tab container during cleanup:",
        error,
      );
    } finally {
      try {
        cleanupOwnedDropIndicator(ownedIndicator);
      } finally {
        this.listenersActive = false;
        this.draggedTabIndex = null;
        this.resetState();
      }
    }
  }

  private restoreOverride(name: string, restore: () => void): void {
    try {
      restore();
    } catch (error) {
      console.error(
        `[TabDragDropManager] Failed to restore ${name}:`,
        error,
      );
    }
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
    const indicator = tabContainer.getElementsByClassName(
      "tab-drop-indicator",
    )[0] as XULElement | undefined;

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
        if (indicator) indicator.hidden = true;
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
      this.groupToInsertTo = tab.parentElement as unknown as XULElement;
      const groupStart = tab.parentElement.querySelector("tab:first-of-type");
      this.positionInGroup = dropIndex -
        Array.prototype.indexOf.call(tabs, groupStart);
    } else {
      this.groupToInsertTo = null;
      this.positionInGroup = null;
    }

    const indicatorTarget = resolveDropIndicatorTarget(dropIndex, tabs.length);
    if (!indicatorTarget) {
      if (indicator) indicator.hidden = true;
      return;
    }

    this.lastKnownIndex = dropIndex;
    if (!indicator) return;

    const ltr = window.getComputedStyle(tabContainer).direction === "ltr";
    const rect = tabContainer.arrowScrollbox.getBoundingClientRect();

    let newMarginX: number;
    let newMarginY: number;
    if (indicatorTarget.atEnd) {
      const tabRect = tabs[indicatorTarget.tabIndex].getBoundingClientRect();
      newMarginX = ltr ? tabRect.right - rect.left : rect.right - tabRect.left;
      newMarginY = tabRect.top + tabRect.height - rect.top - rect.height;
      if (CSS.supports("offset-anchor", "left bottom")) {
        newMarginY += rect.height / 2 - tabRect.height / 2;
      }
    } else {
      const tabRect = tabs[indicatorTarget.tabIndex].getBoundingClientRect();
      newMarginX = ltr ? tabRect.left - rect.left : rect.right - tabRect.right;
      newMarginY = tabRect.top + tabRect.height - rect.top - rect.height;
      if (CSS.supports("offset-anchor", "left bottom")) {
        newMarginY += rect.height / 2 - tabRect.height / 2;
      }
    }

    newMarginX += indicator.clientWidth / 2;
    if (!ltr) newMarginX *= -1;

    const ownedIndicator = this.dropIndicatorOwnership.acquire(indicator);
    ownedIndicator.hidden = false;
    ownedIndicator.style.setProperty(
      "transform",
      `translate(${Math.round(newMarginX)}px, ${Math.round(newMarginY)}px)`,
    );
    ownedIndicator.style.setProperty(
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
        gBrowser.moveTabBefore(tabGroup, tabToMoveTo as unknown as XULElement);
      } else {
        gBrowser.moveTabAfter(tabGroup, tabToMoveTo as unknown as XULElement);
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
            gBrowser.moveTabBefore(t, tabToMoveTo as unknown as XULElement);
          } else {
            gBrowser.moveTabAfter(t, tabToMoveTo as unknown as XULElement);
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
      // NOTE: `ownerGlobal` was removed from XUL elements in recent Firefox.
      // Use `ownerDocument.defaultView` instead to obtain the chrome window.
      const sourceWindow = sourceNode?.ownerDocument?.defaultView as
        | FirefoxWindow
        | undefined;
      if (
        XULElement.isInstance(sourceNode) &&
        sourceNode.localName === "tab" &&
        sourceWindow &&
        sourceNode.ownerDocument?.documentElement &&
        sourceWindow.isChromeWindow &&
        sourceNode.ownerDocument.documentElement.getAttribute("windowtype") ===
          "navigator:browser" &&
        (sourceNode as XULTab).container === sourceWindow.gBrowser.tabContainer
      ) {
        if (
          PrivateBrowsingUtils.isWindowPrivate(window) !==
            PrivateBrowsingUtils.isWindowPrivate(sourceWindow)
        ) {
          return "none";
        }

        if (window.gMultiProcessBrowser !== sourceWindow.gMultiProcessBrowser) {
          return "none";
        }

        if (window.gFissionBrowser !== sourceWindow.gFissionBrowser) {
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
