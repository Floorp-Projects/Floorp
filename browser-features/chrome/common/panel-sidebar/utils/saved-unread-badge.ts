// SPDX-License-Identifier: MPL-2.0

import type { Panel } from "./type.ts";

/** Name the local saved-count preference for a panel. */
function prefName(id: string): string {
  return `floorp.panelSidebar.savedUnread.${encodeURIComponent(id)}`;
}

/** Remove the panel’s saved count from the profile. */
export function removeSavedUnreadBadge(id: string): void {
  Services.prefs.clearUserPref(prefName(id));
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
  const identity = JSON.stringify([panel.url, panel.userContextId ?? 0]);
  const prefix = `${identity}:`;
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
