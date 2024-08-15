/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { StatusBarManager } from "./statusbar-manager";
import { ContextMenu } from "./context-menu";
import { StatusBar } from "./statusbar";

export let manager: StatusBarManager;

export function initStatusbar() {
  createRootHMR(
    () => {
      manager = new StatusBarManager();
    },
    import.meta.hot,
  );
  render(
    StatusBar,
    document.getElementById("navigator-toolbox")?.nextElementSibling,
    { hotCtx: import.meta.hot },
  );
  //https://searchfox.org/mozilla-central/rev/4d851945737082fcfb12e9e7b08156f09237aa70/browser/base/content/main-popupset.js#321
  const mainPopupSet = document.getElementById("mainPopupSet");
  mainPopupSet?.addEventListener("popupshowing", onPopupShowing);

  manager.init();

  import.meta.hot?.accept((m) => {
    m?.initStatusbar();
  });
}

function onPopupShowing(event: Event) {
  switch (event.target.id) {
    case "toolbar-context-menu":
      render(
        ContextMenu,
        document.getElementById("viewToolbarsMenuSeparator")!.parentElement,
        {
          marker: document.getElementById("viewToolbarsMenuSeparator")!,
          hotCtx: import.meta.hot,
        },
      );
  }
}
