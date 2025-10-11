/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { mouseGestureService } from "./service.ts";
export { mouseGestureService } from "./service.ts";
export type {
  GestureAction,
  GestureDirection,
  GesturePattern,
  MouseGestureConfig,
} from "./config.ts";
export {
  getConfig,
  isEnabled,
  patternToString,
  setConfig,
  setEnabled,
  stringToPattern,
} from "./config.ts";
export { GestureDisplay } from "./components/GestureDisplay.tsx";

@noraComponent(import.meta.hot)
export default class MouseGesture extends NoraComponentBase {
  static ctx: typeof mouseGestureService | null = null;

  init(): void {
    const ctx = mouseGestureService;
    MouseGesture.ctx = ctx;
  }
}
