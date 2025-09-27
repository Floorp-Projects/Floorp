import { render } from "@nora/solid-xul";
import { QRCodePageActionButton } from "./qr-code-button.tsx";
import { QRCodePanel } from "./qr-code-panel.tsx";
import { QRCodeManager } from "./qr-code-manager.tsx";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { createRoot, getOwner, runWithOwner } from "solid-js";

export let manager: QRCodeManager;

@noraComponent(import.meta.hot)
export default class QRCodeGenerator extends NoraComponentBase {
  init() {
    manager = new QRCodeManager();
    const mainPopupSet = document?.getElementById("mainPopupSet");
    if (mainPopupSet) {
      render(QRCodePanel, mainPopupSet, {
        hotCtx: import.meta.hot,
      });
    }

    const owner = getOwner();
    globalThis.SessionStore.promiseInitialized.then(() => {
      const exec = () => {
        const starButtonBox = document?.getElementById("star-button-box");
        if (starButtonBox && starButtonBox.parentElement) {
          render(QRCodePageActionButton, starButtonBox.parentElement, {
            marker: starButtonBox,
            hotCtx: import.meta.hot,
          });
        }
      };
      if (owner) runWithOwner(owner, exec);
      else createRoot(exec);
    });

    manager.init();
  }
}
