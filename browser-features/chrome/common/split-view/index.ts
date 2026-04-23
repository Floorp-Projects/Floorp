/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getOwner, runWithOwner, createRoot } from "solid-js";
import { SplitViewManager } from "./split-view-manager.js";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

@noraComponent(import.meta.hot)
export default class SplitView extends NoraComponentBase {
  init(): void {
    const splitViewEnabled = Services.prefs.getBoolPref(
      "browser.tabs.splitView.enabled",
      false,
    );

    if (!splitViewEnabled) {
      this.logger.debug("Split view is disabled upstream, skipping init");
      return;
    }

    this.logger.debug("Initializing advanced split view feature");
    const manager = new SplitViewManager(this.logger);

    // Explicitly pass the reactive owner so createEffect/onCleanup
    // inside SplitViewManager.init() are properly tracked
    const owner = getOwner();
    if (owner) {
      runWithOwner(owner, () => manager.init());
    } else {
      createRoot(() => manager.init());
    }
  }
}
