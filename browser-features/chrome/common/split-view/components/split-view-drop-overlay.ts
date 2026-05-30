/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DropZone } from "../utils/zone-computation.js";

const OVERLAY_ID = "floorp-split-drop-overlay";

function getOrCreateOverlay(): HTMLElement | null {
  const existing = document?.getElementById(OVERLAY_ID);
  if (existing) return existing as HTMLElement;

  const overlay = document?.createElement("div") as HTMLElement;
  if (!overlay) return null;
  overlay.id = OVERLAY_ID;

  const zones: DropZone[] = ["left", "right", "top", "bottom"];
  for (const zone of zones) {
    const el = document?.createElement("div") as HTMLElement;
    if (!el) continue;
    el.className = "floorp-drop-zone";
    el.dataset.zone = zone;
    overlay.appendChild(el);
  }

  // Append to documentElement (not tabpanels) so the overlay is above
  // the browser content area which forwards events to the content process.
  document.documentElement!.appendChild(overlay);
  return overlay;
}

export function showDropOverlay(zone: DropZone): void {
  const overlay = getOrCreateOverlay();
  if (!overlay) return;

  // Position the overlay over the tabpanels rect using tabpanels coordinates
  const tabpanels = document?.getElementById("tabbrowser-tabpanels");
  if (tabpanels) {
    const rect = tabpanels.getBoundingClientRect();
    overlay.style.top = `${rect.top}px`;
    overlay.style.left = `${rect.left}px`;
    overlay.style.width = `${rect.width}px`;
    overlay.style.height = `${rect.height}px`;
  }

  for (const child of overlay.children) {
    const el = child as HTMLElement;
    if (el.dataset.zone === zone) {
      el.setAttribute("data-active", "true");
    } else {
      el.removeAttribute("data-active");
    }
  }

  overlay.setAttribute("data-active-zone", zone);
}

export function hideDropOverlay(): void {
  const overlay = document?.getElementById(OVERLAY_ID);
  if (!overlay) return;

  for (const child of overlay.children) {
    (child as HTMLElement).removeAttribute("data-active");
  }
  overlay.removeAttribute("data-active-zone");
}

export function removeDropOverlay(): void {
  document?.getElementById(OVERLAY_ID)?.remove();
}
