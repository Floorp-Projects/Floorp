/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The two things the rest of the browser can ask Clips to do.
 *
 * The clips themselves live in the Clips page's own storage, which the chrome
 * layer cannot reach into. So "clip this page" leaves the text in a
 * preference and opens the panel; the page picks it up and does the clipping.
 * That also means it works whether or not the panel was already open.
 */

import { PanelSidebarStaticNames } from "../panel-sidebar/utils/panel-sidebar-static-names.ts";

const CLIPS_PANEL_URL = "floorp//clips";
const PENDING_PREF = "floorp.browser.clips.pending";

/** The id of the user's Clips panel, if they still have one. */
function clipsPanelId(): string | null {
  try {
    const raw = Services.prefs.getStringPref(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      "",
    );
    if (!raw) return null;
    const parsed = JSON.parse(raw) as { data?: { id: string; url: string }[] };
    return parsed.data?.find((panel) => panel.url === CLIPS_PANEL_URL)?.id ??
      null;
  } catch (e) {
    console.error("[clips] Could not read the panel sidebar data:", e);
    return null;
  }
}

/**
 * Show the Clips panel.
 *
 * The panel is selected by clicking its own button, the same way a person
 * would — the sidebar keeps its state inside the window it belongs to, and
 * there is no other handle on it from here.
 */
export function openClipsPanel(win: Window): void {
  Services.prefs.setBoolPref(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    true,
  );

  const id = clipsPanelId();
  if (!id) {
    console.warn("[clips] No Clips panel in the sidebar to open.");
    return;
  }

  const button = win.document?.querySelector(
    `.panel-sidebar-panel[data-panel-id="${id}"]`,
  ) as XULElement | null;
  if (!button) return;
  // Already showing: clicking again would close it.
  if (button.getAttribute("data-checked") === "true") return;
  button.click?.();
}

/** Leave the current page for the Clips page to pick up, and show it. */
export function clipCurrentPage(win: Window): void {
  const browser = win.gBrowser?.selectedBrowser;
  const url = browser?.currentURI?.spec;
  if (!url) return;

  const title = win.gBrowser?.selectedTab?.label ?? "";
  Services.prefs.setStringPref(PENDING_PREF, `${title}\n${url}`.trim());
  openClipsPanel(win);
}
