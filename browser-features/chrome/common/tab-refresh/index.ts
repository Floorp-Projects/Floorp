/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import { onCleanup } from "solid-js";
import {
  installHoverReloadController,
  uninstallHoverReloadController,
} from "./controller.ts";
import type {
  HoverReloadBrowser,
  HoverReloadDocument,
  HoverReloadPrefs,
} from "./types.ts";

const LABEL_KEY = "tab-refresh.reload";
const FALLBACK_LABEL = "Reload tab";

function localizedLabel(): string {
  const label = i18next.t(LABEL_KEY);
  return label === LABEL_KEY ? FALLBACK_LABEL : label;
}

function currentBrowser(): HoverReloadBrowser | null {
  return (globalThis as unknown as { gBrowser?: HoverReloadBrowser })
    .gBrowser ??
    null;
}

@noraComponent(import.meta.hot)
export default class TabRefresh extends NoraComponentBase {
  init(): void {
    const browser = currentBrowser();
    const doc = document as HoverReloadDocument | undefined;
    if (!browser || !doc?.documentElement) {
      console.error("[tab-refresh] Browser chrome is unavailable at init.");
      return;
    }

    const controller = installHoverReloadController({
      browser,
      document: doc,
      prefs: Services.prefs as unknown as HoverReloadPrefs,
      label: localizedLabel(),
    });

    addI18nObserver(() => controller.setLabel(localizedLabel()));
    onCleanup(() => uninstallHoverReloadController(controller));
  }
}
