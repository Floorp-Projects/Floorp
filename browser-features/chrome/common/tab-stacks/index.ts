/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import {
  type TabStacksBrowser,
  TabStacksController,
  type TabStacksControllerOptions,
  type TabStacksServices,
} from "./stack-bar.tsx";

export const TAB_STACKS_ENABLED_PREF = "floorp.tabstacks.enabled";

type TabStacksPrefReader = Pick<
  TabStacksServices["prefs"],
  "getBoolPref"
>;

export function isTabStacksFoundationEnabled(
  prefs: TabStacksPrefReader,
): boolean {
  return prefs.getBoolPref(TAB_STACKS_ENABLED_PREF, false);
}

export function initializeTabStacksFoundation(
  options: TabStacksControllerOptions,
): TabStacksController | null {
  if (!isTabStacksFoundationEnabled(options.services.prefs)) {
    return null;
  }

  const controller = new TabStacksController(options);
  controller.init();
  return controller;
}

@noraComponent(import.meta.hot)
export default class TabStacksFoundation extends NoraComponentBase {
  init(): void {
    if (
      typeof document === "undefined" ||
      typeof window === "undefined" ||
      typeof Services === "undefined"
    ) {
      return;
    }

    const controller = initializeTabStacksFoundation({
      document,
      eventTarget: window,
      services: Services as unknown as TabStacksServices,
      getBrowser: () => {
        return (
          (globalThis as unknown as { gBrowser?: TabStacksBrowser }).gBrowser ??
            null
        );
      },
      logger: this.logger,
    });

    if (!controller) {
      this.logger.info(
        "Tab stacks foundation is disabled (floorp.tabstacks.enabled=false).",
      );
      return;
    }

    onCleanup(() => controller.destroy());
  }
}
