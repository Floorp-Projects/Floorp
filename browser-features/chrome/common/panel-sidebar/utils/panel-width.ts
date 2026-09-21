/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const DEFAULT_PANEL_WIDTH = 450;

/**
 * Convert the value returned by the add-panel form into a stored width.
 * Zero is a valid sentinel that makes a panel follow the global width.
 */
export function parsePanelWidth(value: string | number): number {
  if (typeof value === "string" && value.trim() === "") {
    return DEFAULT_PANEL_WIDTH;
  }

  const width = Number(value);
  return Number.isFinite(width) && width >= 0 ? width : DEFAULT_PANEL_WIDTH;
}
