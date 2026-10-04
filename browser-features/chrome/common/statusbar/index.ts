/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/preact-xul";
import { addDisposer } from "@nora/preact-xul/lifetime";
import { ContextMenu } from "./context-menu.tsx";
import { StatusBarElem } from "./statusbar.tsx";
import { StatusBarManager } from "./statusbar-manager.tsx";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

export let manager: StatusBarManager;

@noraComponent("StatusBar", import.meta.hot)
export default class StatusBar extends NoraComponentBase {
  init() {
    manager = new StatusBarManager();
    if (typeof document !== "undefined" && document?.body) {
      const customizationContainer = document.getElementById(
        "customization-container",
      );
      render(StatusBarElem, document.body, { marker: customizationContainer });
      const mainPopupSet = document?.getElementById("mainPopupSet");
      let disposeContextMenu: (() => void) | undefined;
      const handlePopupShowing = (event: Event) => {
        disposeContextMenu ??= onPopupShowing(event);
      };
      mainPopupSet?.addEventListener("popupshowing", handlePopupShowing);
      addDisposer(() => {
        mainPopupSet?.removeEventListener("popupshowing", handlePopupShowing);
        disposeContextMenu?.();
      });
    }
    manager.init();
  }
}

function onPopupShowing(event: Event): (() => void) | undefined {
  if (
    typeof document !== "undefined" &&
    document?.getElementById("toggle_statusBar")
  ) {
    return;
  }

  const target = event.target as HTMLElement | null;
  if (!target || !target.id) {
    return;
  }

  switch (target.id) {
    case "toolbar-context-menu":
      if (typeof document !== "undefined") {
        const separator = document?.getElementById("viewToolbarsMenuSeparator");
        if (separator && separator.parentElement) {
          return render(ContextMenu, separator.parentElement, {
            marker: separator,
          });
        }
      }
      break;
  }
}
