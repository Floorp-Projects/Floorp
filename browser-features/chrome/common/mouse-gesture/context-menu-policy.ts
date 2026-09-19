// SPDX-License-Identifier: MPL-2.0

/** Keep native menus behind mouseup so a drag can finish before they open. */
export function handleContextMenuAfterMouseUp(
  enabled: boolean,
  os: string = Services.appinfo.OS,
): void {
  // Windows always opens context menus on mouseup and ignores this pref.
  if (os === "WINNT") return;

  const pref = "ui.context_menus.after_mouseup";
  // Check the actual preference, not a persisted copy of the feature state.
  // The copy can survive a reset/import of this pref, leaving menus on
  // mousedown even after changing gesture settings or restarting Floorp.
  if (Services.prefs.getBoolPref(pref, false) !== enabled) {
    Services.prefs.setBoolPref(pref, enabled);
  }
}
