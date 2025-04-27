/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { FloorpPrivateContainer } from "./browser-private-container";
import { ContextMenu } from "./context-menu";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

@noraComponent(import.meta.hot)
export default class PrivateContainer extends NoraComponentBase {
  init(): void {
    render(ContextMenu, document.querySelector("#tabContextMenu"), {
      marker: document.querySelector("#context_selectAllTabs")!,
    });
    // Inject menu item to open link in private container.
    window.gFloorpPrivateContainer = new FloorpPrivateContainer();
  }
}
