/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { getConfig, isEnabled, patternToString } from "./config.ts";
import { GestureDisplay } from "./components/GestureDisplay.tsx";
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

    this.targetWindow.addEventListener("mousedown", this.handleMouseDown);
    this.targetWindow.addEventListener("mousemove", this.handleMouseMove);
    this.targetWindow.addEventListener("mouseup", this.handleMouseUp);
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
      this.targetWindow.removeEventListener("mousedown", this.handleMouseDown);
      this.targetWindow.removeEventListener("mousemove", this.handleMouseMove);
      this.targetWindow.removeEventListener("mouseup", this.handleMouseUp);
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

    this.targetWindow.clearTimeout(this.preventionTimeoutId);
    this.preventionTimeoutId = null;
  }

  private scheduleContextMenuPreventionRelease(timeout: number): void {
    this.clearPreventionTimeout();
    this.isContextMenuPrevented = true;
    this.preventionTimeoutId = this.targetWindow.setTimeout(() => {
      this.isContextMenuPrevented = false;
      this.preventionTimeoutId = null;
    }, timeout);
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

  private handleInteractionInterrupted = (): void => {
    this.isContextMenuPrevented = false;
    this.clearPreventionTimeout();
    this.resetGestureState();
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

  private handleMouseDown = (event: MouseEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // A fresh right-button mousedown proves that the previous physical button
    // cycle ended, even if its mouseup was lost while focus was changing.
    if (event.button === 2 && this.isWheelGestureFired) {
      this.resetGestureState();
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
      if (this.pressedButtons.size === 0) {
        this.resetGestureState();
        this.scheduleContextMenuPreventionRelease(
          getConfig().contextMenu.preventionTimeout,
        );
      }
      event.preventDefault();
      event.stopPropagation();
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
        this.display.updateActionName(actionInfo.displayName);

        // Execute the action after a brief display delay
        this.targetWindow.setTimeout(() => {
          executeGestureAction(actionInfo.action, this.targetWindow);
          this.resetGestureState();
          this.scheduleContextMenuPreventionRelease(preventionTimeout);
        }, 100);

        return;
      }
    }

    // No gesture recognized - prevent context menu and reset
    this.resetGestureState();
    this.scheduleContextMenuPreventionRelease(preventionTimeout);
  };

  private handleMouseWheel = (event: WheelEvent): void => {
    if (!isEnabled()) {
      this.resetDisabledState();
      return;
    }

    // After the first wheel action, consume all remaining wheel events in this
    // cycle (including momentum/residual events after mouseup) without firing a
    // second action.
    if (this.isWheelGestureFired || this.isWheelGestureSuppressionActive) {
      event.preventDefault();
      event.stopPropagation();
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

    let action: string | null = null;
    if (event.deltaY < 0) {
      action = "gecko-show-previous-tab";
    } else if (event.deltaY > 0) {
      action = "gecko-show-next-tab";
    }

    if (action) {
      // Set the exact-once latch before executing the action so synchronous
      // re-entrancy cannot execute another wheel action.
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
