// SPDX-License-Identifier: MPL-2.0

const tabIdentities = new WeakMap<XULElement, string>();
let nextTabIdentity = 0;

export function getTabIdentity(tab: XULElement): string {
  let identity = tabIdentities.get(tab);
  if (identity === undefined) {
    identity = `tab-${++nextTabIdentity}`;
    tabIdentities.set(tab, identity);
  }
  return identity;
}

export function getLiveTabIndex(
  tabs: readonly XULElement[],
  tab: XULElement,
): number {
  return isTabOpen(tab) ? tabs.indexOf(tab) : -1;
}

export function isTabOpen(tab: XULElement): boolean {
  return tab.isConnected &&
    (tab as XULElement & { closing?: boolean }).closing !== true;
}
