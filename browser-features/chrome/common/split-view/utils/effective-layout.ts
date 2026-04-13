/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SplitViewLayout } from "../data/types.js";

export function getEffectiveSplitViewLayout(
  layout: SplitViewLayout,
  paneCount: number,
): SplitViewLayout {
  if (layout === "grid-2x2" && paneCount !== 4) {
    return "horizontal";
  }
  if (
    (layout === "grid-3pane-left-main" ||
      layout === "grid-3pane-right-main" ||
      layout === "grid-3pane-top-main" ||
      layout === "grid-3pane-bottom-main") &&
    paneCount !== 3
  ) {
    return "horizontal";
  }
  return layout;
}
