/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { effect } from "@preact/signals";
import { splitViewConfig } from "./data/config.js";
import {
  clearSplitHandles,
  updateHandles,
} from "./components/split-view-splitters.js";
import {
  initLayoutPicker,
  destroyLayoutPicker,
} from "./components/split-view-layout-picker.js";
import {
  initToolbarButtonEnhancement,
  destroyToolbarButtonEnhancement,
} from "./components/split-view-toolbar-button.js";
import splitViewStyles from "./styles/split-view.css?inline";
import { createPatchState } from "./patches/patch-state.js";
import { patchTabpanels } from "./patches/tabpanels-patch.js";
import { patchSplitViewWrapper } from "./patches/wrapper-patch.js";
import { initContextMenu } from "./patches/context-menu.js";
import { initSplitViewEvents } from "./patches/event-listeners.js";
import { initActivePaneTracker } from "./patches/active-pane-tracker.js";
import { initSessionRestore } from "./patches/session-restore.js";
import {
  destroyPaneDrag,
  initPaneDrag,
  updatePaneDragGrips,
} from "./components/split-view-pane-drag.js";
import {
  applyLayout,
  applyLayoutAttribute,
} from "./layout.js";
import type { SplitViewLayout } from "./data/types.js";

/**
 * SplitViewManager orchestrates monkey-patching of Firefox's built-in
 * 2-pane split view system to support N panes (2-4) with multiple layouts.
 */
export class SplitViewManager {
  private styleElement: HTMLStyleElement | null = null;
  private patchState = createPatchState();
  private tabpanelsPatch: { unpatch(): void } | null = null;
  private wrapperPatch: { unpatch(): void } | null = null;
  private contextMenuCleanup: (() => void) | null = null;
  private eventsCleanup: (() => void) | null = null;
  private activePaneCleanup: (() => void) | null = null;
  private sessionRestoreCleanup: (() => void) | null = null;
  private logger: ConsoleInstance;

  constructor(logger: ConsoleInstance) {
    this.logger = logger;
  }

  init(): void {
    this.logger.debug("Initializing SplitViewManager");

    this.injectStyles();

    this.tabpanelsPatch = patchTabpanels(
      this.logger,
      this.patchState,
      (panelIds: string[], layout: SplitViewLayout) => {
        applyLayoutAttribute(this.logger, layout, panelIds.length);
        updateHandles(panelIds, layout);
        updatePaneDragGrips(this.logger, panelIds);
      },
    );

    this.wrapperPatch = patchSplitViewWrapper(this.logger);

    this.contextMenuCleanup = initContextMenu(this.logger);
    initLayoutPicker();
    initToolbarButtonEnhancement();
    this.eventsCleanup = initSplitViewEvents(this.logger);
    this.activePaneCleanup = initActivePaneTracker(this.logger);
    const srCleanup = initSessionRestore(this.logger);
    this.sessionRestoreCleanup = srCleanup ?? null;
    initPaneDrag(this.logger);

    // React to layout config changes
    effect(() => {
      const config = splitViewConfig.value;
      this.logger.debug(
        `[effect] layout config changed: ${config.layout}, maxPanes=${config.maxPanes}`,
      );
      applyLayout(this.logger);

      return () => this.destroy();
    });
  }

  private destroy(): void {
    this.logger.debug("Destroying SplitViewManager");
    this.removeStyles();
    this.tabpanelsPatch?.unpatch();
    this.wrapperPatch?.unpatch();
    this.contextMenuCleanup?.();
    this.eventsCleanup?.();
    this.activePaneCleanup?.();
    this.sessionRestoreCleanup?.();
    destroyPaneDrag();
    clearSplitHandles();
    destroyLayoutPicker();
    destroyToolbarButtonEnhancement();

    // Remove dynamically created context menu items
    document?.getElementById("floorp_openInSplitView")?.remove();
    document?.getElementById("floorp_addPaneToSplitView")?.remove();
    document?.getElementById("floorp_moveTabToPane")?.remove();
  }

  // ===== Style injection =====

  private injectStyles(): void {
    this.styleElement = document?.createElement(
      "style",
    ) as HTMLStyleElement;
    if (this.styleElement) {
      this.styleElement.id = "floorp-split-view-styles";
      this.styleElement.textContent = splitViewStyles;
      document?.head?.appendChild(this.styleElement);
      this.logger.debug("[styles] injected split-view CSS");
    }
  }

  private removeStyles(): void {
    this.styleElement?.remove();
    this.styleElement = null;
  }
}
