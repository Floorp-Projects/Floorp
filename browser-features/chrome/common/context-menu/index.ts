/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { onCleanup } from "solid-js";
import { ContextMenuController } from "./controller.ts";

export * from "./config.ts";
export * from "./types.ts";

@noraComponent(import.meta.hot)
export default class ContextMenu extends NoraComponentBase {
  // NoraComponentBase invokes init() from its constructor. `declare` avoids a
  // derived-class field initializer overwriting the controller created there.
  declare private controller: ContextMenuController | null | undefined;
  declare private cleanupController: (() => void) | undefined;

  init(): void {
    if (this.controller) return;
    const contentAreaContextMenu = ContextMenuUtils.contentAreaContextMenu();
    contentAreaContextMenu?.addEventListener(
      "popupshowing",
      ContextMenuUtils.onPopupShowing,
    );
    this.controller = new ContextMenuController({ window });
    this.controller.attach();

    const cleanup = () => {
      globalThis.removeEventListener("unload", cleanup);
      if (this.cleanupController !== cleanup) return;
      contentAreaContextMenu?.removeEventListener(
        "popupshowing",
        ContextMenuUtils.onPopupShowing,
      );
      this.controller?.destroy();
      this.controller = null;
      this.cleanupController = undefined;
    };
    this.cleanupController = cleanup;
    globalThis.addEventListener("unload", cleanup, { once: true });
    onCleanup(cleanup);
  }
}
