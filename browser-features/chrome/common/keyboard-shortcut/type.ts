/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as t from "io-ts";
import type { GestureActionRegistration } from "../mouse-gesture/utils/gestures.ts";

export const zModifiers = t.type({
  alt: t.boolean,
  ctrl: t.boolean,
  meta: t.boolean,
  shift: t.boolean,
});

export const zShortcutConfig = t.type({
  modifiers: zModifiers,
  key: t.string,
  action: t.string,
});

const zKeyboardShortcutConfigRequired = t.type({
  shortcuts: t.record(t.string, zShortcutConfig),
});

const zKeyboardShortcutConfigOptional = t.partial({
  enabled: t.boolean,
});

export const zKeyboardShortcutConfig = t.intersection([
  zKeyboardShortcutConfigRequired,
  zKeyboardShortcutConfigOptional,
]);

export type Modifiers = t.TypeOf<typeof zModifiers>;
export type ShortcutConfig = t.TypeOf<typeof zShortcutConfig>;
export type KeyboardShortcutConfig = t.TypeOf<typeof zKeyboardShortcutConfig>;
export type { GestureActionRegistration };