/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Pure drag-session state for the chord-drag window move. Kept free of DOM
 * so the gesture rules are unit-testable in the browser harness.
 */

export interface DragSessionState {
  active: boolean;
  lastScreenX: number;
  lastScreenY: number;
}

export type DragSessionAction =
  | { type: "down"; screenX: number; screenY: number }
  | { type: "move"; screenX: number; screenY: number; chordHeld: boolean }
  | { type: "end" }
  | { type: "cancel" };

export type DragSessionResult =
  | { type: "start"; state: DragSessionState }
  | { type: "move"; deltaX: number; deltaY: number; state: DragSessionState }
  | { type: "noop"; state: DragSessionState }
  | { type: "cancel"; state: DragSessionState };

export const emptyDragSession = (): DragSessionState => ({
  active: false,
  lastScreenX: 0,
  lastScreenY: 0,
});

/**
 * Reduce a pointer/keys event into the next drag-session state.
 *
 * - A `down` with the chord held starts a drag anchored at that point.
 * - A `move` while active re-verifies the chord (a mouseup released outside
 *   the window never reaches us; without this a latched drag flag would
 *   relocate the window on every later move).
 * - `end`, or any `cancel` (key-up, blur), stops the drag.
 */
export const reduceDragSession = (
  state: DragSessionState,
  action: DragSessionAction,
): DragSessionResult => {
  switch (action.type) {
    case "down":
      return {
        type: "start",
        state: {
          active: true,
          lastScreenX: action.screenX,
          lastScreenY: action.screenY,
        },
      };
    case "move":
      if (!state.active) {
        return { type: "noop", state };
      }
      if (!action.chordHeld) {
        return {
          type: "cancel",
          state: { ...state, active: false },
        };
      }
      return {
        type: "move",
        deltaX: action.screenX - state.lastScreenX,
        deltaY: action.screenY - state.lastScreenY,
        state: {
          active: true,
          lastScreenX: action.screenX,
          lastScreenY: action.screenY,
        },
      };
    case "end":
      return {
        type: "cancel",
        state: { ...state, active: false },
      };
    case "cancel":
      return {
        type: "cancel",
        state: { ...state, active: false },
      };
  }
};
