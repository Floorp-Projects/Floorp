/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, getOwner, runWithOwner } from "solid-js";
import { isFloatingDragging } from "../data/data";

export function BrowserBox() {
  const owner = getOwner?.();
  const exec = () =>
    createEffect(() => {
      if (isFloatingDragging()) {
        document
          ?.getElementById("panel-sidebar-browser-box-wrapper")
          ?.classList.add("warp");
      } else {
        document
          ?.getElementById("panel-sidebar-browser-box-wrapper")
          ?.classList.remove("warp");
      }
    });
  if (owner) runWithOwner(owner, exec);
  else createRoot(exec);
  return (
    <>
      <xul:box id="panel-sidebar-browser-box-wrapper" />
      <xul:vbox id="panel-sidebar-browser-box" style="flex: 1;" />
    </>
  );
}
