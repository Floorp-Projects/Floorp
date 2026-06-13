/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, getOwner, runWithOwner } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { splitViewEnabled } from "./data/enabled.js";
import { SplitViewManager } from "./split-view-manager.js";

/**
 * Manages SplitViewManager lifecycle in response to enabled pref changes.
 */
export class SplitViewService {
  private manager: SplitViewManager | null = null;
  private managerRootDispose: (() => void) | null = null;
  private serviceRootDispose: (() => void) | null = null;

  constructor(private logger: ConsoleInstance) {}

  init(): void {
    if (this.serviceRootDispose) {
      return;
    }

    const owner = getOwner();
    this.serviceRootDispose = createRoot((dispose) => {
      createEffect(() => {
        if (splitViewEnabled()) {
          this.startManager(owner);
        } else {
          this.stopManager();
        }
      });
      return dispose;
    });
  }

  private startManager(
    owner: NonNullable<ReturnType<typeof getOwner>> | null,
  ): void {
    if (this.manager) {
      return;
    }

    this.logger.debug("Starting SplitViewManager");
    const manager = new SplitViewManager(this.logger);
    this.manager = manager;

    this.managerRootDispose = createRoot((dispose) => {
      if (owner) {
        runWithOwner(owner, () => manager.init());
      } else {
        manager.init();
      }
      return dispose;
    });
  }

  private stopManager(): void {
    if (!this.manager) {
      return;
    }

    this.logger.debug("Stopping SplitViewManager");
    this.manager.destroy();
    this.managerRootDispose?.();
    this.managerRootDispose = null;
    this.manager = null;
  }
}

function createSplitViewService(): SplitViewService {
  const logger = console.createInstance({ prefix: "nora@split-view" });
  return new SplitViewService(logger);
}

export const splitViewService = createRootHMR(
  createSplitViewService,
  import.meta.hot,
);
