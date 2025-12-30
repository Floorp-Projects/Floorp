/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  getConfig,
  isEnabled,
  patternToString,
} from "./config.ts";
import { GestureDisplay } from "./components/GestureDisplay.tsx";
import { executeGestureAction, getActionDisplayName } from "./utils/gestures.ts";
import { createRecognizer, recognize, type ShapeDatabase } from "./utils/recognizer.ts";
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
  private preventionTimeoutId: number | ReturnType<typeof setTimeout> | null = null;
  private mouseTrail: { x: number; y: number }[] = [];
  private display: GestureDisplay;
  private eventListenersAttached = false;
  private pressedButtons = new Set<number>();
  private isRockerGestureFired = false;
  private targetWindow: Window;
  private recognizer: IDollarRecognizer | null = null;
  private shapeDb: ShapeDatabase | null = null;
  private patternActionMap: Map<string, { action: string; displayName: string }> = new Map();
  private lastConfigHash = "";

  constructor(win: Window = globalThis as unknown as Window) {
    this.targetWindow = win;
    this.display = new GestureDisplay(win);
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    this.targetWindow.addEventListener("mousedown", this.handleMouseDown);
    this.targetWindow.addEventListener("mousemove", this.handleMouseMove);
    this.targetWindow.addEventListener("mouseup", this.handleMouseUp);
    this.targetWindow.addEventListener("contextmenu", this.handleContextMenu, true);
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener("mousedown", this.handleMouseDown);
      this.targetWindow.removeEventListener("mousemove", this.handleMouseMove);
      this.targetWindow.removeEventListener("mouseup", this.handleMouseUp);
      this.targetWindow.removeEventListener("contextmenu", this.handleContextMenu, true);
      this.eventListenersAttached = false;
    }

    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.resetGestureState();
    this.display.destroy();
  }

  /**
   * Get or create the $1 Recognizer, rebuilding if config changed.
   * Also builds the pattern-to-action lookup map for fast access.
   */
  private getRecognizerAndShapeDb(): { recognizer: IDollarRecognizer; shapeDb: ShapeDatabase } {
    const config = getConfig();
    const configHash = JSON.stringify(config.actions);

    if (!this.recognizer || !this.shapeDb || this.lastConfigHash !== configHash) {
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
    const sensitivity = Number.isFinite(config.sensitivity) ? config.sensitivity : 40;
    const sensitivityFactor = Math.min(Math.max(sensitivity, 1), 100) / 100;
    // Higher sensitivity = lower required score (easier to match)
    return Math.max(0.5, 0.85 - sensitivityFactor * 0.3);
  }

  /**
   * Calculate the minimum movement distance to trigger recognition.
   */
  private getActivationDistance(): number {
    const config = getConfig();
    const baseDistance = config.contextMenu?.minDistance ?? 10;
    const sensitivity = Number.isFinite(config.sensitivity) ? config.sensitivity : 40;
    const sensitivityFactor = Math.min(Math.max(sensitivity, 1), 100) / 100;
    const dynamicDistance = 6 + (1 - sensitivityFactor) * 12;
    return Math.max(baseDistance, dynamicDistance, 10);
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

  private resetGestureState(): void {
    this.isGestureActive = false;
    this.isRockerGestureFired = false;
    this.mouseTrail = [];
    this.display.hide();
    this.pressedButtons.clear();
  }

  private getViewportPointFromEvent(event: MouseEvent): { x: number; y: number } {
    // Prefer Firefox's content-area screen offsets when available so we can
    // convert absolute screen coordinates into viewport coordinates.
    // Fallback to client coordinates if not available.
    const win = this.targetWindow as unknown as Window & {
      mozInnerScreenX?: number;
      mozInnerScreenY?: number;
    };
    if (typeof (win as any).mozInnerScreenX === "number" && typeof (win as any).mozInnerScreenY === "number") {
      return {
        x: event.screenX - (win as any).mozInnerScreenX,
        y: event.screenY - (win as any).mozInnerScreenY,
      };
    }
    return { x: event.clientX, y: event.clientY };
  }

  private handleMouseDown = (event: MouseEvent): void => {
    if (!isEnabled()) return;

    this.pressedButtons.add(event.button);
    const config = getConfig();

    // Handle rocker gestures (left+right mouse buttons)
    if (config.rockerGesturesEnabled) {
      const LEFT = 0;
      const RIGHT = 2;
      let action: string | null = null;

      // Right button held, then left button pressed -> back
      if (this.isGestureActive && event.button === LEFT) {
        action = "gecko-back";
      }
      // Left button held, then right button pressed -> forward
      else if (this.pressedButtons.has(LEFT) && event.button === RIGHT) {
        action = "gecko-forward";
      }

      if (action) {
        executeGestureAction(action);
        event.preventDefault();
        event.stopPropagation();
        this.isRockerGestureFired = true;
        this.isContextMenuPrevented = true;
        return;
      }
    }

    // Only start gesture on right mouse button
    if (event.button !== 2 || this.isGestureActive) return;

    this.isContextMenuPrevented = true;
    if (this.preventionTimeoutId !== null) {
      clearTimeout(this.preventionTimeoutId);
      this.preventionTimeoutId = null;
    }

    this.isGestureActive = true;
    this.mouseTrail = [this.getViewportPointFromEvent(event)];

    this.display.show();
    this.display.updateTrail(this.mouseTrail);
  };

  private handleMouseMove = (event: MouseEvent): void => {
    if (!this.isGestureActive || !isEnabled()) return;

    // Collect trail point (use browser-global -> viewport mapping)
    this.mouseTrail.push(this.getViewportPointFromEvent(event));
    this.display.updateTrail(this.mouseTrail);

    // Perform real-time recognition for instant feedback
    const totalMovement = this.getTotalMovement();
    const activationDistance = this.getActivationDistance();

    if (totalMovement >= activationDistance) {
      const { recognizer, shapeDb } = this.getRecognizerAndShapeDb();
      const minScore = this.getMinScore();
      const result = recognize(recognizer, this.mouseTrail, minScore, shapeDb);

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

    // Handle rocker gesture cleanup
    if (this.isRockerGestureFired) {
      if (this.pressedButtons.size === 0) {
        this.resetGestureState();
        this.isContextMenuPrevented = true;
        if (this.preventionTimeoutId) clearTimeout(this.preventionTimeoutId);
        this.preventionTimeoutId = this.targetWindow.setTimeout(() => {
          this.isContextMenuPrevented = false;
          this.preventionTimeoutId = null;
        }, getConfig().contextMenu.preventionTimeout);
      }
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    if (!this.isGestureActive || event.button !== 2 || !isEnabled()) return;

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
    const result = recognize(recognizer, this.mouseTrail, minScore, shapeDb);

    if (result) {
      // Use cached pattern-to-action map for fast lookup
      const actionInfo = this.patternActionMap.get(result.patternName);

      if (actionInfo) {
        this.display.updateActionName(actionInfo.displayName);

        // Execute the action after a brief display delay
        this.targetWindow.setTimeout(() => {
          executeGestureAction(actionInfo.action);
          this.resetGestureState();
          this.preventionTimeoutId = this.targetWindow.setTimeout(() => {
            this.isContextMenuPrevented = false;
            this.preventionTimeoutId = null;
          }, preventionTimeout);
        }, 100);

        return;
      }
    }

    // No gesture recognized - prevent context menu and reset
    this.preventionTimeoutId = this.targetWindow.setTimeout(() => {
      this.isContextMenuPrevented = false;
      this.preventionTimeoutId = null;
    }, preventionTimeout);

    this.resetGestureState();
  };

  private handleContextMenu = (event: MouseEvent): void => {
    if ((this.isGestureActive || this.isContextMenuPrevented) && isEnabled()) {
      event.preventDefault();
      event.stopPropagation();
    }
  };
}
