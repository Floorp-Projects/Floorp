// SPDX-License-Identifier: MPL-2.0

import type { Panel } from "./type.ts";

const SAVED_UNREAD_PREFIX = "floorp.panelSidebar.savedUnread.";

/** Name the local saved-count preference for a panel. */
function prefName(id: string): string | undefined {
  if (!id.isWellFormed()) return undefined;
  return `${SAVED_UNREAD_PREFIX}${encodeURIComponent(id)}`;
}

/** Remove the panel’s saved count from the profile. */
export function removeSavedUnreadBadge(id: string): void {
  const name = prefName(id);
  if (name) Services.prefs.clearUserPref(name);
}

/** Identify the URL and container associated with a saved count. */
function badgeIdentityPrefix(panel: Panel): string {
  return `${JSON.stringify([panel.url, panel.userContextId ?? 0])}:`;
}

/** Remove saved counts whose panel or matching URL and container no longer exist. */
export function pruneSavedUnreadBadges(panels: readonly Panel[]): void {
  const livePrefs = new Map(panels.map((panel) => [prefName(panel.id), panel]));
  for (const name of Services.prefs.getChildList(SAVED_UNREAD_PREFIX)) {
    const panel = livePrefs.get(name);
    if (
      panel?.type !== "web" || !panel.loadAtStartupForBadges ||
      !Services.prefs.getStringPref(name, "").startsWith(
        badgeIdentityPrefix(panel),
      )
    ) {
      Services.prefs.clearUserPref(name);
    }
  }
}

/** Access the saved count for an opted-in Google Messages panel. */
export function savedUnreadBadgeForPanel(panel: Panel) {
  if (URL.parse(panel.url ?? "")?.origin !== "https://messages.google.com") {
    return;
  }
  const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  );
  if (PrivateBrowsingUtils.isWindowPrivate(window)) return;
  if (!panel.loadAtStartupForBadges) {
    removeSavedUnreadBadge(panel.id);
    return;
  }

  const name = prefName(panel.id);
  if (!name) return;
  const prefix = badgeIdentityPrefix(panel);
  const stored = Services.prefs.getStringPref(name, "");
  const count = stored.startsWith(prefix) ? stored.slice(prefix.length) : "";
  return {
    count: /^(?:[1-9][0-9]?|99\+)$/.test(count) ? count : "",
    write(count: string): void {
      if (count) Services.prefs.setStringPref(name, prefix + count);
      else removeSavedUnreadBadge(panel.id);
    },
  };
}
