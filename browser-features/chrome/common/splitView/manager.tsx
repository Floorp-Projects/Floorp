/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Show } from "solid-js/web";
import { Popup } from "./popup";
import { currentSplitView } from "./utils/data";
import { render } from "@nora/solid-xul";
import icon from "./assets/link.svg?raw";
import type { CSplitView } from "./splitView";

export class SplitViewManager {
  constructor(ctx: CSplitView) {
    const parent = this.targetParent;
    if (!parent) {
      console.error(
        "[SplitViewManager] Target parent not found; toolbar element will not be rendered.",
      );
      return;
    }

    const marker = this.markerElement;
    if (!marker) {
      console.warn(
        "[SplitViewManager] Marker element not found; toolbar element will be appended at the end.",
      );
    } else if (marker.parentElement !== parent) {
      console.warn(
        "[SplitViewManager] Marker element parent mismatch; toolbar element will be appended at the end.",
      );
    }

    try {
      render(() => this.ToolbarElement({ ctx }), parent, {
        marker: marker?.parentElement === parent ? marker : undefined,
      });
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[SplitViewManager] Failed to render toolbar element.",
        reason,
      );
    }
  }

  private get targetParent() {
    return document?.querySelector(
      ".urlbar-input-container[flex='1']",
    ) as XULElement;
  }

  private get markerElement() {
    return document?.querySelector(
      ".urlbar-searchmode-and-input-box-container",
    ) as XULElement;
  }

  private ToolbarElement(props: { ctx: CSplitView }) {
    return (
      <Show when={currentSplitView() !== -1}>
        <xul:hbox
          class="urlbar-page-action"
          role="button"
          popup="splitView-panel"
          id="splitView-action"
        >
          <xul:image
            id="splitView-image"
            class="urlbar-icon"
            style={{
              "list-style-image": `url("data:image/svg+xml,${
                encodeURIComponent(icon)
              }")`,
            }}
          />
          <Popup ctx={props.ctx} />
        </xul:hbox>
      </Show>
    );
  }
}
