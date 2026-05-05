/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { addDisposer } from "@nora/preact-xul/lifetime";
import {
  isModalVisible,
  modalSize,
  type ModalSize,
} from "./data/data.ts";
import type { TForm, TFormResult } from "./utils/type";

export class ModalManager {
  private static get targetParent(): HTMLElement | null {
    return document?.getElementById("main-window") as HTMLElement | null;
  }

  constructor() {
    const handleKeydown = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isModalVisible.value) {
        this.hide();
      }
    };
    globalThis.addEventListener("keydown", handleKeydown);
    addDisposer(() => globalThis.removeEventListener("keydown", handleKeydown));
  }

  public show(
    form: TForm,
    options: { width: number; height: number },
  ): Promise<TFormResult | null> | undefined {
    const container = document?.getElementById(
      "modal-parent-container",
    ) as unknown as XULElement;
    if (container) {
      isModalVisible.value = true;
      modalSize.value = {
        width: options.width,
        height: options.height,
      };
      container.focus();

      const browser = document?.getElementById(
        "modal-child-browser",
      // deno-lint-ignore no-explicit-any
      ) as unknown as XULElement & { browsingContext: any };

      const actor = browser.browsingContext.currentWindowGlobal.getActor(
        "NRChromeModal",
      );

      const safeForm = JSON.parse(JSON.stringify(form));

      return new Promise((resolve) => {
        actor
          .sendQuery("NRChromeModal:show", safeForm)
          .then((response: TFormResult | null) => {
            resolve(response);
          });
      });
    }
  }

  public hide(): void {
    const browser = document?.getElementById(
      "modal-parent-container",
    ) as unknown as XULElement;
    if (browser) {
      isModalVisible.value = false;
      modalSize.value = { width: 600, height: 800 };
      globalThis.focus();
      Services.obs.notifyObservers({}, "nora:modal:hide", "");
    }
  }

  public setModalSize(newSize: ModalSize): void {
    modalSize.value = { ...modalSize.value, ...newSize };
  }

  public handleBackdropClick(_event: MouseEvent): void {
    //TODO: Make more stable
    // const target = event.target as HTMLElement;
    // if (target.id === "modal-parent-container") {
    //   this.hide();
    // }
  }

  public static get parentElement(): HTMLElement | null {
    return this.targetParent;
  }
}
