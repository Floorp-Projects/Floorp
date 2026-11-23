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
import type {
  ChromeWebStoreWindow,
  CWSObserver,
  InstallInfoState,
} from "./types";
import { NotificationCustomizer } from "./notification-customizer";
import {
  createCWSObserver,
  overrideInstallConfirmation,
  removeCWSObserver,
} from "./observer";

@noraComponent(import.meta.hot)
export default class Addons extends NoraComponentBase {
  // NoraComponentBase calls init synchronously from super(). Do not overwrite
  // resources created there with derived field initializers afterward.
  declare private state: InstallInfoState;
  declare private cwsObserver: CWSObserver | null;
  declare private customizer: NotificationCustomizer | null;
  declare private cleanupInstallConfirmation: (() => void) | null;
  declare private cleanupLoad: (() => void) | null;
  declare private disposed: boolean | undefined;

  constructor(lifecycleTarget?: EventTarget | null) {
    super(lifecycleTarget);
  }

  protected get ownerWindow(): ChromeWebStoreWindow {
    return window;
  }

  init() {
    const win = this.ownerWindow;
    if (this.disposed !== undefined || win.closed) return;
    this.disposed = false;
    this.state = { pendingChromeWebStoreInstall: null };
    this.cwsObserver = null;
    this.cleanupInstallConfirmation = null;
    this.cleanupLoad = null;
    this.customizer = new NotificationCustomizer(win);
    onCleanup(() => this.cleanup());
    // Set up observer first
    this.cwsObserver = createCWSObserver(
      this.state,
      this.customizer,
      this.logger,
      win,
    );

    // Wait for the window to be fully loaded
    if (win.document.readyState === "complete") {
      this.initCustomization();
    } else {
      const onLoad = () => this.initCustomization();
      win.addEventListener("load", onLoad, { once: true });
      this.cleanupLoad = () => win.removeEventListener("load", onLoad);
    }
  }

  private initCustomization(): void {
    if (this.disposed || this.ownerWindow.closed || !this.customizer) return;
    this.cleanupLoad?.();
    this.cleanupLoad = null;
    this.cleanupInstallConfirmation?.();
    this.cleanupInstallConfirmation = overrideInstallConfirmation(
      this.state,
      this.customizer,
      this.logger,
      this.ownerWindow,
    );
  }

  private cleanup(): void {
    this.disposed = true;
    this.cleanupLoad?.();
    this.cleanupLoad = null;

    if (this.cleanupInstallConfirmation) {
      this.cleanupInstallConfirmation();
      this.cleanupInstallConfirmation = null;
    }
    removeCWSObserver(this.cwsObserver);
    this.cwsObserver = null;
    this.customizer?.destroy();
    this.customizer = null;
    this.state.pendingChromeWebStoreInstall = null;
  }
}
