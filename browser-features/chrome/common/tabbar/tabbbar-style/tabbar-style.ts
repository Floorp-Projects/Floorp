/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, getOwner, runWithOwner } from "solid-js";
import { gTabbarStyleFunctions } from "./tabbbar-style-functions";

export class TabbarStyleClass {
  private get tabbarWindowManageContainer() {
    return document?.querySelector(
      "#TabsToolbar > .titlebar-buttonbox-container",
    );
  }

  constructor() {
    this.tabbarWindowManageContainer?.setAttribute(
      "id",
      "floorp-tabbar-window-manage-container",
    );

    gTabbarStyleFunctions.applyTabbarStyle();

    const owner = getOwner?.();
    const exec = () =>
      createEffect(() => {
        gTabbarStyleFunctions.applyTabbarStyle();
      });
    if (owner) runWithOwner(owner, exec);
    else createRoot(exec);
  }
}
