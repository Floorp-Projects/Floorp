/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { insert, render } from "@nora/solid-xul";
import { gFloorpStatusBar } from "./browser-statusbar";
import { ContextMenu } from "./context-menu";
import { StatusBar } from "./statusbar";

export function initStatusbar() {
  render(() => <StatusBar />, document.getElementById("navigator-toolbox"));
  insert(
    document.getElementById("toolbar-context-menu"),
    () => <ContextMenu />,
    document.getElementById("viewToolbarsMenuSeparator"),
  );

  // Initialize statusbar
  gFloorpStatusBar.getInstance();
}
