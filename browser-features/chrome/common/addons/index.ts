/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Chrome Web Store Install Confirmation UI Customization
 *
 * This module modifies the addon installation confirmation popup
 * to show Chrome extension specific messages when installing from
 * Chrome Web Store.
 */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import type { ChromeWebStoreInstallInfo, CWSObserver } from "./types";
import { NotificationCustomizer } from "./notification-customizer";
import {
  createCWSObserver,
  overrideInstallConfirmation,
  removeCWSObserver,
} from "./observer";

@noraComponent(import.meta.hot)
export default class Addons extends NoraComponentBase {
  private state: {
    pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
  } = {
    pendingChromeWebStoreInstall: null,
  };
  private cwsObserver: CWSObserver | null = null;
  private customizer = new NotificationCustomizer();
  private cleanupInstallConfirmation: (() => void) | null = null;

  init() {
    this.logger.info("Initializing Chrome Web Store install customization");

    // Set up observer first
    this.cwsObserver = createCWSObserver(
      this.state,
      this.customizer,
      this.logger,
    );

    // Wait for the window to be fully loaded
    if (document.readyState === "complete") {
      this.initCustomization();
    } else {
      globalThis.addEventListener("load", () => this.initCustomization(), {
        once: true,
      });
    }

    onCleanup(() => {
      this.cleanup();
    });
  }

  private initCustomization(): void {
    this.cleanupInstallConfirmation = overrideInstallConfirmation(
      this.state,
      this.customizer,
      this.logger,
    );
  }

  private cleanup(): void {
    removeCWSObserver(this.cwsObserver);
    this.cwsObserver = null;

    if (this.cleanupInstallConfirmation) {
      this.cleanupInstallConfirmation();
      this.cleanupInstallConfirmation = null;
    }
  }
}
