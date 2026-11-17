// SPDX-License-Identifier: MPL-2.0

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import {
  installTabLoadPreserveController,
  uninstallTabLoadPreserveController,
} from "./controller.ts";
import type {
  PreserveLoadDocument,
  PreserveLoadTabBrowser,
  PreserveLoadUrlbar,
} from "./types.ts";

interface BrowserChromeGlobals {
  gBrowser?: PreserveLoadTabBrowser;
  gURLBar?: PreserveLoadUrlbar;
  isBlankPageURL?: (url: string) => boolean;
  isInitialPage?: (url: string) => boolean;
}

@noraComponent(import.meta.hot)
export default class TabLoadPreserve extends NoraComponentBase {
  init(): void {
    const globals = globalThis as unknown as BrowserChromeGlobals;
    const tabBrowser = globals.gBrowser;
    if (!tabBrowser?.addTabsProgressListener || !tabBrowser.tabContainer) {
      console.error("[tab-load-preserve] gBrowser is unavailable at init.");
      return;
    }

    const doc = (typeof document !== "undefined" ? document : null) as
      | PreserveLoadDocument
      | null;

    const controller = installTabLoadPreserveController({
      tabBrowser,
      urlbar: globals.gURLBar ?? null,
      document: doc,
      isBlankPageURL: globals.isBlankPageURL,
      isInitialPage: globals.isInitialPage,
    });

    onCleanup(() => uninstallTabLoadPreserveController(controller));
  }
}
