// SPDX-License-Identifier: MPL-2.0

type TabWithHiddenAttribute = {
  hasAttribute(name: string): boolean;
};

/**
 * Firefox ignores hidden tabs when deciding whether a tab is the window's
 * last. Keep native last-tab closing enabled unless another hidden tab must be
 * preserved; otherwise Firefox would close the window and discard that tab.
 */
export function hasHiddenTabToPreserve<T extends TabWithHiddenAttribute>(
  tabs: Iterable<T>,
  closingTab?: T,
): boolean {
  for (const tab of tabs) {
    if (tab !== closingTab && tab.hasAttribute("hidden")) {
      return true;
    }
  }
  return false;
}

/**
 * When another window keeps Firefox's profile-wide last-tab preference off,
 * Firefox creates a replacement tab in this window. Take the native window
 * close path only when the Workspace exit setting permits it and this window
 * has no hidden tab to preserve.
 */
export function shouldCloseWindowForLastTabReplacement<
  T extends TabWithHiddenAttribute,
>(
  tabs: Iterable<T>,
  closingTab: T,
  exitOnLastTabClose: boolean,
): boolean {
  return exitOnLastTabClose &&
    !hasHiddenTabToPreserve(tabs, closingTab);
}

/**
 * The close-with-last-tab preference is profile-wide, so native closing is
 * safe only when no browser window contains a hidden tab that must survive.
 */
export function hasAnyWindowWithHiddenTabToPreserve<
  T extends TabWithHiddenAttribute,
>(
  tabsByWindow: Iterable<Iterable<T>>,
  closingTab?: T,
): boolean {
  for (const tabs of tabsByWindow) {
    if (hasHiddenTabToPreserve(tabs, closingTab)) {
      return true;
    }
  }
  return false;
}
