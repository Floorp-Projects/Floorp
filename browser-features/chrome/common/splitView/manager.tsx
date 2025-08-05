/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Show } from "solid-js/web";
import { Popup } from "./popup";
import { currentSplitView } from "./utils/data";
import { render } from "@nora/solid-xul";
import { CSplitView } from "./splitView";

export class SplitViewManager {
  constructor(ctx: CSplitView) {
    render(() => this.ToolbarElement({ ctx }), this.targetParent, {
      marker: this.markerElement,
    });
  }

  private get targetParent() {
    return document?.querySelector(
      ".urlbar-input-container[flex='1']",
    ) as XULElement;
  }

  private get markerElement() {
    return document?.getElementById(
      ".urlbar-searchmode-and-input-box-container",
    ) as XULElement;
  }

  private ToolbarElement(props: { ctx: CSplitView }) {
    return (
      <Show when={currentSplitView() !== -1}>
        <xul:hbox
          data-l10n-id="splitView-action"
          class="urlbar-page-action"
          role="button"
          popup="splitView-panel"
          id="splitView-action"
          hidden={false}
        >
          <xul:image id="splitView-image" class="urlbar-icon" />
          <Popup ctx={props.ctx} />
        </xul:hbox>
      </Show>
    );
  }
}
