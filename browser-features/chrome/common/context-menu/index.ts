/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { onCleanup } from "solid-js";
import { ContextMenuRuntime } from "./runtime.ts";
import { FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE } from "./style.ts";

export * from "./config.ts";
export * from "./types.ts";

@noraComponent(import.meta.hot)
export default class ContextMenu extends NoraComponentBase {
  // NoraComponentBase invokes init() from its constructor. `declare` avoids a
  // derived-class field initializer overwriting the runtime created there.
  declare private runtime: ContextMenuRuntime | null | undefined;
  declare private cleanupController: (() => void) | undefined;

  init(): void {
    if (this.runtime) return;
    // This native menu repair predates customization and remains available
    // regardless of enrollment. Only the customizer's controller is gated.
    const popup = ContextMenuUtils.contentAreaContextMenu();
    popup?.addEventListener("popupshowing", ContextMenuUtils.onPopupShowing);
    this.runtime = new ContextMenuRuntime({ window });
    this.runtime.start();

    const cleanup = () => {
      globalThis.removeEventListener("unload", cleanup);
      if (this.cleanupController !== cleanup) return;
      this.runtime?.destroy();
      popup?.removeEventListener(
        "popupshowing",
        ContextMenuUtils.onPopupShowing,
      );
      for (
        const separator of popup?.querySelectorAll(
          `[${FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE}]`,
        ) ?? []
      ) {
        (separator as XULElement).hidden = false;
        separator.removeAttribute(FLOORP_LEGACY_SEPARATOR_HIDDEN_ATTRIBUTE);
      }
      this.runtime = null;
      this.cleanupController = undefined;
    };
    this.cleanupController = cleanup;
    globalThis.addEventListener("unload", cleanup, { once: true });
    onCleanup(cleanup);
  }
}
