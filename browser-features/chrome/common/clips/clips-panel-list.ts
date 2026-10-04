/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Clips rides a Flasco. FloorpClipsGate (modules layer) leaves its verdict in
 * `floorp.browser.clips.enabled` at startup; the chrome layer reads it here,
 * once, when it loads — so the panel list, the static panel table and the
 * actions all agree for the whole run.
 */

import type { Panel, Panels } from "../panel-sidebar/utils/type.ts";

export const CLIPS_ENABLED_PREF = "floorp.browser.clips.enabled";
export const CLIPS_PANEL_URL = "floorp//clips";
export const CLIPS_PANEL_ID = "default-panel-clips";
const NOTES_PANEL_URL = "floorp//notes";

export function clipsEnabled(): boolean {
  try {
    return Services.prefs.getBoolPref(CLIPS_ENABLED_PREF, false);
  } catch (e) {
    console.error("[clips] Could not read the Clips pref:", e);
    return false;
  }
}

/**
 * The user's panel list with the Clips panel present or absent. Defaults are
 * copied once, so someone enrolled later has no entry: add it after Notes,
 * where the sketch puts it, and take it out again when the Flasco says so.
 */
export function withClipsPanel(
  panels: Panels,
  enabled: boolean,
): { panels: Panels; changed: boolean } {
  const without = panels.filter((p) => p.url !== CLIPS_PANEL_URL);
  if (!enabled) {
    return { panels: without, changed: without.length !== panels.length };
  }
  if (without.length !== panels.length) return { panels, changed: false };
  const entry: Panel = {
    id: CLIPS_PANEL_ID,
    url: CLIPS_PANEL_URL,
    width: 0,
    type: "static",
    icon: undefined,
    userContextId: undefined,
    zoomLevel: undefined,
    userAgent: undefined,
    extensionId: undefined,
  };
  const notes = without.findIndex((p) => p.url === NOTES_PANEL_URL);
  const at = notes === -1 ? without.length : notes + 1;
  return {
    panels: [...without.slice(0, at), entry, ...without.slice(at)],
    changed: true,
  };
}
