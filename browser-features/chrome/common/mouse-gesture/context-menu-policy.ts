// SPDX-License-Identifier: MPL-2.0

/** Keep native menus behind mouseup so a drag can finish before they open. */
export function handleContextMenuAfterMouseUp(
  enabled: boolean,
  os: string = Services.appinfo.OS,
  prefs: Pick<
    nsIPrefBranch,
    "getBoolPref" | "setBoolPref" | "prefIsLocked"
  > = Services.prefs,
): void {
  // Windows always opens context menus on mouseup and ignores this pref.
  if (os === "WINNT") return;

  const pref = "ui.context_menus.after_mouseup";
  // Check the actual preference, not a persisted copy of the feature state.
  // The copy can survive a reset/import of this pref, leaving menus on
  // mousedown even after changing gesture settings or restarting Floorp.
  try {
    if (prefs.getBoolPref(pref, false) === enabled) return;
    if (prefs.prefIsLocked(pref)) {
      console.error(
        "[MouseGestureService] Native menu timing preference is locked:",
        pref,
      );
      return;
    }
    prefs.setBoolPref(pref, enabled);
  } catch (error) {
    // A failed preference write must not interrupt controller/observer setup.
    console.error(
      "[MouseGestureService] Failed to update native menu timing:",
      error,
    );
  }
}
