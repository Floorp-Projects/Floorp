/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getConfig, isEnabled, patternToString } from "./config.ts";
import { GestureDisplay } from "./components/GestureDisplay.tsx";
import { normalizeWheelActions } from "./wheel-action-policy.ts";
import {
  executeGestureAction,
  getActionDisplayName,
} from "./utils/gestures.ts";
import {
  createRecognizer,
  recognize,
  type ShapeDatabase,
} from "./utils/recognizer.ts";
import type { IDollarRecognizer } from "./utils/dollar.ts";

/**
 * MouseGestureController handles mouse gesture recognition.
 *
 * This controller uses the $1 Unistroke Recognizer algorithm:
 * - Collects mouse trail points during right-click drag
 * - Performs real-time recognition during drag for instant feedback
 * - Executes the action when the gesture is complete (on mouse up)
 */
export class MouseGestureController {
  private isGestureActive = false;
  private isContextMenuPrevented = false;
  private preventionTimeoutId: number | null = null;
  private isWheelGestureFired = false;
  private isWheelGestureSuppressionActive = false;
  private wheelGestureSuppressionTimeoutId: number | null = null;
  private mouseTrail: { x: number; y: number }[] = [];
  private display: GestureDisplay;
  private eventListenersAttached = false;
  private pressedButtons = new Set<number>();
  private isRockerGestureFired = false;
  private targetWindow: Window;
  private recognizer: IDollarRecognizer | null = null;
  private shapeDb: ShapeDatabase | null = null;
  private patternActionMap: Map<
    string,
    { action: string; displayName: string }
  > = new Map();
  private lastConfigHash = "";

  constructor(win: Window = globalThis as unknown as Window) {
    this.targetWindow = win;
    this.display = new GestureDisplay(win);
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) {
      return;
    }

    // Capture-phase mousedown/mousemove/mouseup: content scripts (e.g. video
    // players, or any page that stops propagation on its own mouse handlers)
    // may call stopPropagation(), which would otherwise prevent rocker- and
    // drawn-gesture detection from ever running, and would prevent the
    // wheel-gesture cleanup / context-menu suppression from starting
    // (Floorp issue #2586).
    this.targetWindow.addEventListener(
      "mousedown",
      this.handleMouseDown,
      true,
    );
    this.targetWindow.addEventListener(
      "mousemove",
      this.handleMouseMove,
      true,
    );
    this.targetWindow.addEventListener("mouseup", this.handleMouseUp, true);
    this.targetWindow.addEventListener(
      "contextmenu",
      this.handleContextMenu,
      true,
    );
    this.targetWindow.addEventListener("wheel", this.handleMouseWheel, {
      passive: false,
    });
    this.targetWindow.addEventListener(
      "blur",
      this.handleInteractionInterrupted,
    );
    this.targetWindow.addEventListener(
      "pagehide",
      this.handleInteractionInterrupted,
    );
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "mousedown",
        this.handleMouseDown,
        true,
      );
      this.targetWindow.removeEventListener(
        "mousemove",
        this.handleMouseMove,
        true,
      );
      this.targetWindow.removeEventListener(
        "mouseup",
        this.handleMouseUp,
        true,
      );
      this.targetWindow.removeEventListener(
        "contextmenu",
        this.handleContextMenu,
        true,
      );
      this.targetWindow.removeEventListener("wheel", this.handleMouseWheel);
      this.targetWindow.removeEventListener(
        "blur",
        this.handleInteractionInterrupted,
      );
      this.targetWindow.removeEventListener(
        "pagehide",
        this.handleInteractionInterrupted,
      );
      this.eventListenersAttached = false;
    }

    this.clearPreventionTimeout();

    this.resetGestureState();
    this.display.destroy();
  }

  /**
   * Get or create the $1 Recognizer, rebuilding if config changed.
   * Also builds the pattern-to-action lookup map for fast access.
   */
  private getRecognizerAndShapeDb(): {
    recognizer: IDollarRecognizer;
    shapeDb: ShapeDatabase;
  } {
    const config = getConfig();
    const configHash = JSON.stringify(config.actions);

    if (
      !this.recognizer ||
      !this.shapeDb ||
      this.lastConfigHash !== configHash
    ) {
      const result = createRecognizer(config.actions);
      this.recognizer = result.recognizer;
      this.shapeDb = result.shapeDb;
      this.lastConfigHash = configHash;

      // Build pattern-to-action map for fast lookup
      this.patternActionMap.clear();
      for (const action of config.actions) {
        const patternKey = patternToString(action.pattern);
        this.patternActionMap.set(patternKey, {
          action: action.action,
          displayName: getActionDisplayName(action.action),
        });
      }
    }

    return { recognizer: this.recognizer, shapeDb: this.shapeDb };
  }

  /**
   * Calculate minimum score threshold based on sensitivity setting.
   */
  private getMinScore(): number {
    const config = getConfig();
    const sensitivity = Number.isFinite(config.sensitivity)
      ? config.sensitivity
      : 40;
    const sensitivityFactor = Math.min(Math.max(sensitivity, 1), 100) / 100;
    // Higher sensitivity = lower required score (easier to match)
    return Math.max(0.5, 0.85 - sensitivityFactor * 0.3);
  }

  /**
   * Calculate the minimum movement distance to trigger recognition.
   * Uses the user-configured minDistance directly.
   */
  private getActivationDistance(): number {
    const config = getConfig();
    return config.contextMenu?.minDistance ?? 10;
  }

  /**
   * Calculate total movement distance from start to end of trail.
   */
  private getTotalMovement(): number {
    if (this.mouseTrail.length < 2) return 0;

    const startPoint = this.mouseTrail[0];
    const lastPoint = this.mouseTrail[this.mouseTrail.length - 1];

    const dx = lastPoint.x - startPoint.x;
    const dy = lastPoint.y - startPoint.y;

    return Math.sqrt(dx * dx + dy * dy);
  }

  private clearPreventionTimeout(): void {
    if (this.preventionTimeoutId === null) {
      return;
    }

    const timeoutId = this.preventionTimeoutId;
    this.preventionTimeoutId = null;
    try {
      this.targetWindow.clearTimeout(timeoutId);
    } catch {
      // A gesture action can synchronously close its browser window. There is
      // no timeout left to clear once that window's timer APIs are unavailable.
    }
  }

  private scheduleContextMenuPreventionRelease(timeout: number): void {
    this.clearPreventionTimeout();
    this.isContextMenuPrevented = true;
    try {
      if (!this.eventListenersAttached || this.targetWindow.closed) {
        this.isContextMenuPrevented = false;
        return;
      }
      this.preventionTimeoutId = this.targetWindow.setTimeout(() => {
        this.isContextMenuPrevented = false;
        this.preventionTimeoutId = null;
      }, timeout);
    } catch {
      // A closed window can reject new timers. Do not leave a permanent
      // suppression latch behind when a bounded release cannot be scheduled.
      this.isContextMenuPrevented = false;
      this.preventionTimeoutId = null;
    }
  }

  private clearWheelGestureState(): void {
    if (this.wheelGestureSuppressionTimeoutId !== null) {
      this.targetWindow.clearTimeout(this.wheelGestureSuppressionTimeoutId);
      this.wheelGestureSuppressionTimeoutId = null;
    }

    this.isWheelGestureFired = false;
    this.isWheelGestureSuppressionActive = false;
  }

  private startWheelGestureSuppression(timeout: number): void {
    this.clearWheelGestureState();
    this.isWheelGestureSuppressionActive = true;
    this.wheelGestureSuppressionTimeoutId = this.targetWindow.setTimeout(() => {
      this.isWheelGestureSuppressionActive = false;
      this.wheelGestureSuppressionTimeoutId = null;
    }, timeout);
  }

  private resetDisabledState(): void {
    this.resetInteractionState();
  }

  private resetInteractionState(): void {
    this.isContextMenuPrevented = false;
    this.clearPreventionTimeout();
    this.resetGestureState();
  }

  private resetGestureState(): void {
    this.isGestureActive = false;
    this.isRockerGestureFired = false;
    this.clearWheelGestureState();
    this.mouseTrail = [];
    this.display.hide();
    this.pressedButtons.clear();
  }

  private handleInteractionInterrupted = (event: Event): void => {
    // Gecko fires a top-level "blur" on this window as a side effect of
    // focus moving between browser elements during a tab close/switch (most
    // visibly when the closed tab is replaced by a differently-privileged
    // empty/new-tab page), even though the OS-level window focus never
    // actually left the browser. Only treat this as a genuine interruption
    // (alt-tab, switching to another application/window) when the OS focus
    // really did leave — otherwise this wipes a gesture the user just
    // started mid-drag.
    if (
      event.type === "blur" && Services.focus.activeWindow === this.targetWindow
    ) {
      return;
    }
    this.resetInteractionState();
  };

  private getViewportPointFromEvent(event: MouseEvent): {
    x: number;
    y: number;
  } {
    // Prefer Firefox's content-area screen offsets when available so we can
    // convert absolute screen coordinates into viewport coordinates.
    // Fallback to client coordinates if not available.
    const win = this.targetWindow as unknown as Window & {
      mozInnerScreenX?: number;
      mozInnerScreenY?: number;
    };
    if (
      typeof win.mozInnerScreenX === "number" &&
      typeof win.mozInnerScreenY === "number"
    ) {
      return {
        x: event.screenX - win.mozInnerScreenX,
        y: event.screenY - win.mozInnerScreenY,
      };
    }
    return { x: event.clientX, y: event.clientY };
  }

  private preventFollowingClick(event: MouseEvent): void {
    const geckoEvent = event as MouseEvent & {
      preventClickEvent?: () => void;
    };
    geckoEvent.preventClickEvent?.();
  }

  private isSecondaryButtonPhysicallyDown(event: MouseEvent): boolean {
    return (event.buttons & 2) !== 0;
  }

  private handleMouseDown = (event: MouseEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // A stale gesture can survive same-window focus churn if its physical
    // releases were lost. Reconcile the stored state with Gecko's physical
    // right-button bit before rocker detection so the user's next ordinary
    // left click cannot be swallowed or misread as another rocker action.
    if (
      (this.isGestureActive ||
        this.isWheelGestureFired ||
        this.isRockerGestureFired) &&
      event.button !== 2 &&
      !this.isSecondaryButtonPhysicallyDown(event)
    ) {
      this.resetInteractionState();
    }

    // A fresh right-button mousedown proves that the previous physical button
    // cycle ended, even if its mouseup was lost while focus was changing.
    if (
      event.button === 2 &&
      (this.isWheelGestureFired ||
        this.isGestureActive ||
        (this.isRockerGestureFired && (event.buttons & 1) === 0))
    ) {
      this.resetInteractionState();
    }

    this.pressedButtons.add(event.button);

    // Once a wheel gesture has fired, it owns the remainder of this right-button
    // cycle. Do not allow a later button press to turn it into a rocker gesture.
    if (this.isWheelGestureFired) {
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    const config = getConfig();

    // Handle rocker gestures (left+right mouse buttons)
    if (config.rockerGesturesEnabled) {
      const LEFT = 0;
      const RIGHT = 2;
      let action: string | null = null;

      // Right button held, then left button pressed -> use configured action
      if (this.isGestureActive && event.button === LEFT) {
        action = config.rockerActions.rightLeft;
      } else if (
        // Left button held, then right button pressed -> use configured action
        this.pressedButtons.has(LEFT) && event.button === RIGHT
      ) {
        action = config.rockerActions.leftRight;
      }

      if (action) {
        this.preventFollowingClick(event);
        executeGestureAction(action, this.targetWindow);
        event.preventDefault();
        event.stopPropagation();
        this.isRockerGestureFired = true;
        this.isContextMenuPrevented = true;
        return;
      }
    }

    // Only start gesture on right mouse button
    if (event.button !== 2 || this.isGestureActive) return;

    // A new right-button cycle supersedes any bounded suppression left by the
    // previous wheel gesture.
    this.clearWheelGestureState();
    this.isContextMenuPrevented = true;
    this.clearPreventionTimeout();

    this.isGestureActive = true;
    this.mouseTrail = [this.getViewportPointFromEvent(event)];

    this.display.show();
    this.display.updateTrail(this.mouseTrail);
  };

  private handleMouseMove = (event: MouseEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // A rocker action already owns this button cycle. A move with no physical
    // buttons proves that all of its releases were lost; clear it passively.
    // With one side still held, preserve ownership until the final release.
    if (this.isRockerGestureFired && event.buttons === 0) {
      this.resetInteractionState();
      return;
    }

    // Left unhandled, these
    // movement events pass straight through to the page and let its native
    // text-selection drag keep extending for as long as both buttons stay
    // down (visibly so if the rocker action scrolls the page underneath the
    // still-held cursor) - selecting the page instead of just running the
    // gesture.
    if (this.isRockerGestureFired) {
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    // The controller may have missed the physical right-button mouseup while
    // the active browser window was replacing or closing a tab. A later move
    // with no secondary-button bit proves the stored trail is stale.
    if (
      this.isGestureActive &&
      !this.isWheelGestureFired &&
      !this.isSecondaryButtonPhysicallyDown(event)
    ) {
      this.resetInteractionState();
      return;
    }

    // Wheel gestures are discrete and must not fall through to drawn gesture
    // recognition if the pointer moves before the right button is released.
    if (!this.isGestureActive || this.isWheelGestureFired) return;

    // Collect trail point (use browser-global -> viewport mapping)
    const point = this.getViewportPointFromEvent(event);

    // Skip points with negligible movement to reduce noise
    const last = this.mouseTrail[this.mouseTrail.length - 1];
    if (last) {
      const dx = point.x - last.x;
      const dy = point.y - last.y;
      if (Math.hypot(dx, dy) < 1.5) {
        return;
      }
    }

    this.mouseTrail.push(point);

    // Keep the trail size bounded to avoid excessive redraw/recognition cost
    const MAX_POINTS = 600;
    if (this.mouseTrail.length > MAX_POINTS) {
      const stride = Math.ceil(this.mouseTrail.length / 400);
      this.mouseTrail = this.mouseTrail.filter(
        (_, idx) => idx % stride === 0 || idx === this.mouseTrail.length - 1,
      );
    }
    this.display.updateTrail(this.mouseTrail);

    // Perform real-time recognition for instant feedback
    const totalMovement = this.getTotalMovement();
    const activationDistance = this.getActivationDistance();

    if (totalMovement >= activationDistance) {
      const { recognizer, shapeDb } = this.getRecognizerAndShapeDb();
      const minScore = this.getMinScore();
      const result = recognize(
        recognizer,
        this.mouseTrail,
        minScore,
        shapeDb,
        activationDistance,
      );

      if (result) {
        // Use cached pattern-to-action map for fast lookup
        const actionInfo = this.patternActionMap.get(result.patternName);
        if (actionInfo) {
          this.display.updateActionName(actionInfo.displayName);
        } else {
          this.display.updateActionName("");
        }
      } else {
        this.display.updateActionName("");
      }
    }
  };

  private handleMouseUp = (event: MouseEvent): void => {
    this.pressedButtons.delete(event.button);

    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // Complete a wheel gesture on right-button release without entering the
    // zero-movement drawn-gesture path. The separate suppression state remains
    // alive long enough to cover Firefox's post-mouseup contextmenu event and
    // any residual wheel events, but does not permit another action.
    if (this.isWheelGestureFired) {
      this.preventFollowingClick(event);

      if (event.button === 2) {
        const preventionTimeout = getConfig().contextMenu.preventionTimeout;
        this.isContextMenuPrevented = false;
        this.clearPreventionTimeout();
        this.resetGestureState();
        this.startWheelGestureSuppression(preventionTimeout);
      }

      event.preventDefault();
      event.stopPropagation();
      return;
    }

    // Handle rocker gesture cleanup
    if (this.isRockerGestureFired) {
      this.preventFollowingClick(event);

      // For a leftRight rocker (left pressed first), the left mousedown is
      // never prevented - a lone left click must still behave normally - so
      // the browser already started real native selection-drag tracking on
      // press. That tracking only ends once the page actually receives the
      // matching left mouseup, so it must be let through. `isGestureActive`
      // is exactly the signal for which case this is: it's still true here
      // only for a rightLeft rocker (right pressed first, which does start
      // the normal drawn-gesture path), where the left mousedown *was*
      // already prevented when it completed the combo - so unlike leftRight,
      // there's no unblocked native default for that mouseup to terminate,
      // and it should stay suppressed like every other button. Captured
      // before resetGestureState() below clears it.
      const shouldSuppressMouseUp = event.button !== 0 || this.isGestureActive;
      // `buttons` is the authoritative post-release physical state. This also
      // completes cleanup if an earlier release was lost and therefore remains
      // stale in pressedButtons.
      if (event.buttons === 0) {
        this.resetGestureState();
        this.scheduleContextMenuPreventionRelease(
          getConfig().contextMenu.preventionTimeout,
        );
      }
      if (shouldSuppressMouseUp) {
        event.preventDefault();
        event.stopPropagation();
      }
      return;
    }

    if (!this.isGestureActive || event.button !== 2) return;

    const config = getConfig();
    const preventionTimeout = config.contextMenu.preventionTimeout;

    // Check if we moved enough to be considered a gesture
    const totalMovement = this.getTotalMovement();
    const activationDistance = this.getActivationDistance();

    if (totalMovement < activationDistance) {
      // Not enough movement - allow context menu
      this.isContextMenuPrevented = false;
      this.resetGestureState();
      return;
    }

    // Use $1 Recognizer to identify the gesture
    const { recognizer, shapeDb } = this.getRecognizerAndShapeDb();
    const minScore = this.getMinScore();
    const result = recognize(
      recognizer,
      this.mouseTrail,
      minScore,
      shapeDb,
      activationDistance,
    );

    if (result) {
      // Use cached pattern-to-action map for fast lookup
      const actionInfo = this.patternActionMap.get(result.patternName);

      if (actionInfo) {
        const action = actionInfo.action;
        this.display.updateActionName(actionInfo.displayName);

        // Prevent Gecko's trusted follow-up auxclick and make the controller
        // non-reentrant before tab-closing actions can spin a nested event loop.
        this.preventFollowingClick(event);
        event.preventDefault();
        event.stopPropagation();
        this.resetGestureState();

        // Keep suppression active throughout the synchronous action. Closing
        // a tab/window may run nested timers and dispatch contextmenu before
        // the action returns; starting the release timer beforehand lets that
        // nested loop expire the latch too early.
        this.isContextMenuPrevented = true;
        try {
          executeGestureAction(action, this.targetWindow);
        } finally {
          this.scheduleContextMenuPreventionRelease(preventionTimeout);
        }
        return;
      }
    }

    // No gesture recognized - prevent context menu and reset
    this.preventFollowingClick(event);
    event.preventDefault();
    event.stopPropagation();
    this.resetGestureState();
    this.scheduleContextMenuPreventionRelease(preventionTimeout);
  };

  private handleMouseWheel = (event: WheelEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // Consume residual wheel events only while post-mouseup suppression is
    // active (momentum / touchpad residual events). While the right button is
    // still held, each wheel notch must keep switching tabs — the exact-once
    // latch introduced in #2559 made only a single tab switch possible per
    // right-button cycle (Floorp issue #2586).
    if (this.isWheelGestureSuppressionActive) {
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    // A wheel/drawn cycle cannot remain active without the physical right
    // button. Treat a residual wheel after a lost mouseup as evidence that the
    // cycle ended; it must be passive and must not execute another action.
    if (
      (this.isGestureActive || this.isWheelGestureFired) &&
      !this.isRockerGestureFired &&
      !this.isSecondaryButtonPhysicallyDown(event)
    ) {
      this.resetInteractionState();
      return;
    }

    // A leftRight rocker can legitimately continue with only the left button
    // held. Only a no-buttons wheel proves that the entire rocker cycle ended.
    if (this.isRockerGestureFired && event.buttons === 0) {
      this.resetInteractionState();
      return;
    }

    // A rocker action already owns this button cycle.
    if (this.isRockerGestureFired) {
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    if (!this.isGestureActive) {
      return;
    }

    const config = getConfig();
    if (!config.wheelGesturesEnabled) {
      return;
    }

    // Defend the execution boundary as well as preference parsing. Tests,
    // extensions, or future callers can update the in-memory config directly;
    // none may turn repeatable wheel input into a destructive action.
    const wheelActions = normalizeWheelActions(config.wheelActions);
    let action: string | null = null;
    if (event.deltaY < 0) {
      action = wheelActions.scrollUp;
    } else if (event.deltaY > 0) {
      action = wheelActions.scrollDown;
    }

    if (action) {
      // Mark the cycle as a wheel gesture so the mouseup handler can start
      // the context-menu suppression window. This does not latch: subsequent
      // wheel events while the button is held execute further actions
      // (multi-tab switching, Floorp issue #2586).
      this.isWheelGestureFired = true;
      this.isContextMenuPrevented = false;
      this.clearPreventionTimeout();
      executeGestureAction(action, this.targetWindow);
      event.preventDefault();
      event.stopPropagation();
    }
  };

  private handleContextMenu = (event: MouseEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // Keyboard context-menu input (or any other event with no physical right
    // button) must not remain blocked by a gesture whose releases were lost.
    if (
      ((this.isGestureActive || this.isWheelGestureFired) &&
        !this.isRockerGestureFired &&
        !this.isSecondaryButtonPhysicallyDown(event)) ||
      (this.isRockerGestureFired && event.buttons === 0)
    ) {
      this.resetInteractionState();
    }

    if (
      this.isGestureActive ||
      this.isContextMenuPrevented ||
      this.isWheelGestureFired ||
      this.isWheelGestureSuppressionActive
    ) {
      event.preventDefault();
      event.stopPropagation();
    }
  };
}
