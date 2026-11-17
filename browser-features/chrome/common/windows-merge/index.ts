// SPDX-License-Identifier: MPL-2.0

/**
 * "Merge all windows" — pulls every tab of every other Firefox window into the
 * window it is invoked from, then closes the emptied windows.
 *
 * Private and non-private windows are never mixed: a private window is skipped
 * when the invoking window is not private, and vice versa.
 *
 * Reachable as:
 *  - the tab context menu entry below "Select All Tabs" (tab-context-menu.tsx),
 *  - the `floorp-merge-all-windows` action in the command palette (F2),
 *  - a keyboard shortcut or mouse gesture bound to that action
 *    (see mouse-gesture/utils/actions.ts for the static catalog entry).
 */

import { onCleanup } from "solid-js";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { installMergeAllWindowsMenuItem } from "./tab-context-menu.tsx";

@noraComponent(import.meta.hot)
export default class WindowsMerge extends NoraComponentBase {
  init(): void {
    if (typeof document === "undefined") {
      this.logger.warn("No document; skipping the tab context menu entry.");
      return;
    }

    const dispose = installMergeAllWindowsMenuItem(
      document.getElementById("tabContextMenu"),
    );
    onCleanup(dispose);
  }
}
