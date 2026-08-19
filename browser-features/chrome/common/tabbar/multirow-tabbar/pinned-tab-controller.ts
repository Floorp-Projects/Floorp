import { findChildIndex } from "./dom-utils.ts";

type TabbrowserTabElement = Element & { closing?: boolean };

function isMigratablePinnedTab(
  candidate: Node | null,
  sourceContainer: Element,
): candidate is TabbrowserTabElement {
  return candidate instanceof Element &&
    candidate.parentElement === sourceContainer &&
    candidate.isConnected &&
    candidate.matches(".tabbrowser-tab[pinned]") &&
    (candidate as TabbrowserTabElement).closing !== true;
}

export class PinnedTabController {
  private mutationObserver: MutationObserver | null = null;
  private isRegistered = false;

  constructor(private readonly resolveTabsContainer: () => XULElement | null) {}

  register(): void {
    if (this.isRegistered) return;

    const tabsContainer = this.resolveTabsContainer();
    const pinnedTabsContainer = document?.getElementById(
      "pinned-tabs-container",
    );

    if (!tabsContainer || !pinnedTabsContainer) {
      return;
    }

    this.mutationObserver = new MutationObserver((mutationList) => {
      for (const mutation of mutationList) {
        this.migratePinnedTabs(
          tabsContainer,
          mutation.addedNodes,
          pinnedTabsContainer,
        );
      }
    });

    this.mutationObserver.observe(pinnedTabsContainer, { childList: true });

    // Migrate existing pinned tabs
    if (pinnedTabsContainer.childElementCount > 0) {
      this.migratePinnedTabs(
        tabsContainer,
        pinnedTabsContainer.childNodes,
        pinnedTabsContainer,
      );
    }

    gBrowser.tabContainer.addEventListener(
      "TabUnpinned",
      this.handleTabUnpinned,
      false,
    );

    this.isRegistered = true;
  }

  unregister(): void {
    if (!this.isRegistered) return;

    if (this.mutationObserver) {
      this.mutationObserver.disconnect();
      this.mutationObserver = null;
    }

    gBrowser.tabContainer.removeEventListener(
      "TabUnpinned",
      this.handleTabUnpinned,
      false,
    );

    this.isRegistered = false;
  }

  migratePinnedTabs(
    newContainer: Element,
    candidates: Iterable<Node | null>,
    sourceContainer: Element,
  ): void {
    for (const candidate of Array.from(candidates)) {
      if (!isMigratablePinnedTab(candidate, sourceContainer)) {
        continue;
      }

      const tab = candidate;
      tab.setAttribute("newPin", "true");

      const firstUnpinnedTab = newContainer.querySelector(
        ".tabbrowser-tab:not([pinned])",
      );
      const periphery = document?.getElementById(
        "tabbrowser-arrowscrollbox-periphery",
      );

      if (firstUnpinnedTab) {
        newContainer.insertBefore(tab, firstUnpinnedTab);
      } else if (periphery) {
        newContainer.insertBefore(tab, periphery);
      }
    }
  }

  private handleTabUnpinned = (event: Event): void => {
    this.fixUnpinnedTabsPosition(event);
  };

  private fixUnpinnedTabsPosition(event: Event): void {
    const tab = event.target as Element;
    tab.removeAttribute("newPin");

    const tabsContainer = this.resolveTabsContainer();
    if (!tabsContainer) return;

    const pinnedTabs = tabsContainer.querySelectorAll(
      ".tabbrowser-tab[pinned]",
    );
    if (!pinnedTabs || pinnedTabs.length === 0) return;

    const lastPinnedTab = pinnedTabs[pinnedTabs.length - 1];
    const indexToInsertBefore = findChildIndex(tabsContainer, lastPinnedTab) +
      1;

    tabsContainer.insertBefore(
      tab,
      tabsContainer.childNodes[indexToInsertBefore],
    );
  }
}
