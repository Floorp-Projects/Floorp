/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createRoot, getOwner, runWithOwner } from "solid-js";
import { isFloatingDragging } from "../data/data.ts";

export function BrowserBox() {
  const owner = getOwner?.();
  const exec = () => {
    const el =
      globalThis.document?.getElementById(
        "panel-sidebar-browser-box-wrapper",
      ) ?? null;
    return createEffect(() => {
      el?.classList.toggle("warp", isFloatingDragging());
    });
  };
  if (owner) runWithOwner(owner, exec);
  else createRoot(exec);
  return (
    <>
      <xul:box id="panel-sidebar-browser-box-wrapper" />
      <xul:vbox id="panel-sidebar-browser-box" style="flex: 1;" />
    </>
  );
}
