/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { keyboardShortcutService } from "./service.ts";
export { keyboardShortcutService } from "./service.ts";
export type {
  KeyboardShortcutConfig,
  Modifiers,
  ShortcutConfig,
} from "./type.ts";
export {
  getConfig,
  isEnabled,
  setConfig,
  setEnabled,
  shortcutToString,
  stringToShortcut,
} from "./config.ts";

@noraComponent(import.meta.hot)
export default class KeyboardShortcut extends NoraComponentBase {
  static ctx: typeof keyboardShortcutService | null = null;
  init(): void {
    const ctx = keyboardShortcutService;
    KeyboardShortcut.ctx = ctx;
    ctx.attachToWindow(window);
  }
}
