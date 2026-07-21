// SPDX-License-Identifier: MPL-2.0

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { render } from "@nora/solid-xul";
import { createRoot, getOwner, onCleanup, runWithOwner } from "solid-js";
import { SilverPassPageActionButton } from "./silver-pass-button.tsx";
import { SilverPassManager } from "./silver-pass-manager.ts";
import { SilverPassPanel } from "./silver-pass-panel.tsx";
import style from "./style.css?inline";

@noraComponent(import.meta.hot)
export default class SilverPassDemo extends NoraComponentBase {
  init(): void {
    const manager = new SilverPassManager();
    let disposed = false;

    const existingStyle = document?.getElementById("silver-pass-demo-style");
    existingStyle?.remove();
    const styleElement = document?.createElement("style");
    if (styleElement) {
      styleElement.id = "silver-pass-demo-style";
      styleElement.textContent = style;
      document?.head?.appendChild(styleElement);
    }

    const mainPopupSet = document?.getElementById("mainPopupSet");
    if (mainPopupSet && !document?.getElementById("silver-pass-panel")) {
      render(() => SilverPassPanel({ manager }), mainPopupSet, {
        hotCtx: import.meta.hot,
      });
    }

    const owner = getOwner();
    globalThis.SessionStore.promiseInitialized.then(() => {
      if (disposed || document?.getElementById("SilverPassPageAction")) return;
      const renderButton = () => {
        const marker = document?.getElementById("star-button-box");
        if (!marker?.parentElement) return;
        render(SilverPassPageActionButton, marker.parentElement, {
          marker,
          hotCtx: import.meta.hot,
        });
      };
      if (owner) runWithOwner(owner, renderButton);
      else createRoot(renderButton);
    }).catch((error: unknown) => {
      console.error("[SilverPass] SessionStore initialization failed:", error);
    });

    onCleanup(() => {
      disposed = true;
      manager.destroy();
      styleElement?.remove();
    });
  }
}
