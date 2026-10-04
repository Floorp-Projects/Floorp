/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SplitViewTab } from "../data/types.js";

const DRAG_SERVICE_CONTRACT_ID = "@mozilla.org/widget/dragservice;1";

/** Gecko internals used only when the native dragend event was lost. */
export interface NativeTabDragController {
  finishMoveTogetherSelectedTabs?: (tab: RecoverableTab) => void;
  finishAnimateTabMove?: () => void;
  _resetTabsAfterDrop?: (ownerDocument: Document) => void;
}

export interface RecoverableTab extends SplitViewTab {
  _dragData?: object;
  closing?: boolean;
  container?: {
    tabDragAndDrop?: NativeTabDragController;
  };
  isConnected?: boolean;
  ownerDocument?: Document;
}

/**
 * Native identities captured for one Floorp drag transaction. These
 * references must still be identical when recovery is attempted.
 */
export interface NativeTabDragRecovery {
  readonly tab: RecoverableTab;
  readonly dragData: object;
  readonly controller: NativeTabDragController;
  readonly ownerDocument: Document;
  dragendObserved: boolean;
  tabGoneObserved: boolean;
  terminalGuardUsed: boolean;
}

export type DragSessionReader = () => nsIDragSession | null;

export type NativeTabDragRecoveryResult =
  | "full"
  | "ui-only-tab-gone"
  | "active-session"
  | "blocked";

/**
 * Capture the native identities without mutating them. Missing state is not
 * an error: normal bubbling dragend remains the primary finalization path.
 */
export function captureNativeTabDragRecovery(
  tab: SplitViewTab,
): NativeTabDragRecovery | null {
  try {
    const recoverableTab = tab as RecoverableTab;
    const dragData = recoverableTab._dragData;
    const controller = recoverableTab.container?.tabDragAndDrop;
    const ownerDocument = recoverableTab.ownerDocument;

    if (!dragData || !controller || !ownerDocument) {
      return null;
    }

    return {
      tab: recoverableTab,
      dragData,
      controller,
      ownerDocument,
      dragendObserved: false,
      tabGoneObserved: false,
      terminalGuardUsed: false,
    };
  } catch {
    return null;
  }
}

/** Read the platform drag session through Gecko's supported drag service. */
export function getCurrentNativeDragSession(): nsIDragSession | null {
  const dragService = Cc[DRAG_SERVICE_CONTRACT_ID].getService(
    Ci.nsIDragService,
  );
  return dragService.getCurrentSession() ?? null;
}

/**
 * Recover a native tab drag only after Gecko failed to deliver dragend.
 *
 * Every prerequisite is checked before the first private finalizer is called.
 * An active native drag session is retryable and does not consume the terminal
 * guard. Once the session is gone, the guard is consumed before the first
 * private call so a re-entrant or repeated attempt cannot invoke the sequence
 * twice. Live tabs receive Gecko's full finalizer order. Closing or
 * disconnected tabs receive controller-level UI cleanup through the captured
 * controller even when Gecko has already removed or replaced `tab.container`.
 */
export function recoverLostNativeTabDrag(
  captured: NativeTabDragRecovery,
  current: NativeTabDragRecovery | null,
  readSession: DragSessionReader = getCurrentNativeDragSession,
): NativeTabDragRecoveryResult {
  if (
    current !== captured ||
    captured.dragendObserved ||
    captured.terminalGuardUsed
  ) {
    return "blocked";
  }

  try {
    const { tab, dragData, controller, ownerDocument } = captured;
    const tabIsGone = captured.tabGoneObserved || tab.closing === true ||
      tab.linkedBrowser === null || tab.isConnected === false;

    // The document and native drag payload identify the captured transaction
    // on both paths. A live tab additionally has to retain the exact current
    // controller; a gone tab may no longer have a container, so its captured
    // controller is the only safe UI-cleanup target.
    if (tab.ownerDocument !== ownerDocument || tab._dragData !== dragData) {
      return "blocked";
    }
    if (!tabIsGone && tab.container?.tabDragAndDrop !== controller) {
      return "blocked";
    }

    const finishAnimateTabMove = controller.finishAnimateTabMove;
    const resetTabsAfterDrop = controller._resetTabsAfterDrop;
    if (
      typeof finishAnimateTabMove !== "function" ||
      typeof resetTabsAfterDrop !== "function"
    ) {
      return "blocked";
    }

    let finishMoveTogetherSelectedTabs:
      | ((recoveringTab: RecoverableTab) => void)
      | null = null;
    if (!tabIsGone) {
      const candidate = controller.finishMoveTogetherSelectedTabs;
      if (typeof candidate !== "function") {
        return "blocked";
      }
      finishMoveTogetherSelectedTabs = candidate;
    }

    if (readSession() !== null) {
      return "active-session";
    }

    captured.terminalGuardUsed = true;
    if (finishMoveTogetherSelectedTabs) {
      finishMoveTogetherSelectedTabs.call(controller, tab);
    }
    finishAnimateTabMove.call(controller);
    resetTabsAfterDrop.call(controller, ownerDocument);
    if (tab._dragData === dragData) {
      delete tab._dragData;
    }
    return tabIsGone ? "ui-only-tab-gone" : "full";
  } catch {
    // Do not continue a partially failed private sequence. The caller will
    // discard the pending split and perform Floorp-owned UI cleanup only.
    return "blocked";
  }
}
