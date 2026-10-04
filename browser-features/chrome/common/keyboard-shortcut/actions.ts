/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { gestureActions } from "../mouse-gesture/utils/gestures.ts";
import { openInlineTabUrlEditorForSelectedTab } from "../tab-inline-edit/controller.ts";
import type {
  KeyboardActionFn,
  KeyboardOnlyActionRegistration,
} from "./type.ts";

export const INLINE_TAB_URL_ACTION_ID = "floorp-edit-tab-url" as const;

export const KEYBOARD_ONLY_ACTIONS: readonly KeyboardOnlyActionRegistration[] =
  Object.freeze([
    Object.freeze({
      name: INLINE_TAB_URL_ACTION_ID,
      fn: (win: Window): void => {
        openInlineTabUrlEditorForSelectedTab(win);
      },
    }),
  ]);

const keyboardOnlyActionMap = new Map(
  KEYBOARD_ONLY_ACTIONS.map((action) => [action.name, action.fn]),
);

export function getKeyboardOnlyActions(): readonly KeyboardOnlyActionRegistration[] {
  return KEYBOARD_ONLY_ACTIONS;
}

export function getKeyboardShortcutAction(
  actionId: string,
): KeyboardActionFn | undefined {
  return keyboardOnlyActionMap.get(actionId) ??
    gestureActions.getAction(actionId);
}
