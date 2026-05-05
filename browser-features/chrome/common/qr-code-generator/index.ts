import { h, render } from "preact";
import { safeRender } from "@nora/preact-xul";
import { QRCodePageActionButton } from "./qr-code-button.tsx";
import { QRCodePanel } from "./qr-code-panel.tsx";
import style from "./style.css?inline";
import { QRCodeManager } from "./qr-code-manager.tsx";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base";

export let manager: QRCodeManager;

@noraComponent("QRCodeGenerator", import.meta.hot)
export default class QRCodeGenerator extends NoraComponentBase {
  init() {
    manager = new QRCodeManager();
    // Attach local styles once
    const styleEl = document?.createElement("style");
    if (styleEl) {
      styleEl.textContent = style;
      document?.head?.appendChild(styleEl);
    }
    const mainPopupSet = document?.getElementById("mainPopupSet");
    if (mainPopupSet) {
      safeRender(h(QRCodePanel, null), mainPopupSet);
    }

    globalThis.SessionStore.promiseInitialized.then(() => {
      const starButtonBox = document?.getElementById("star-button-box");
      if (starButtonBox && starButtonBox.parentElement) {
        safeRender(h(QRCodePageActionButton, null), starButtonBox.parentElement);
      }
    });

    manager.init();
  }
}
