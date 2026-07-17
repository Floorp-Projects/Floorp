/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { onCleanup } from "solid-js";
import {
  isModalVisible,
  type ModalSize,
  setModalSize,
  setModalVisible,
} from "./data/data.ts";
import type { TForm, TFormResult } from "./utils/type.ts";

interface BrowsingContextLike {
  currentWindowGlobal: {
    getActor(name: string): {
      sendQuery(message: string, data: unknown): Promise<unknown>;
    };
  };
}

export class ModalManager {
  private static get targetParent(): HTMLElement | null {
    return document?.getElementById("main-window") as HTMLElement | null;
  }

  constructor() {
    const handleKeydown = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isModalVisible()) {
        this.hide();
      }
    };
    globalThis.addEventListener("keydown", handleKeydown);
    onCleanup(() => globalThis.removeEventListener("keydown", handleKeydown));
  }

  public show(
    form: TForm,
    options: { width: number; height: number },
  ): Promise<TFormResult | null> {
    const container = document?.getElementById(
      "modal-parent-container",
    ) as unknown as XULElement;
    if (container) {
      setModalVisible(true);
      setModalSize({
        width: options.width,
        height: options.height,
      });
      container.focus();

      const browser = document?.getElementById(
        "modal-child-browser",
      ) as unknown as XULElement & { browsingContext: BrowsingContextLike } | null;

      if (!browser) {
        // Never leave callers awaiting undefined — that used to flow an
        // undefined result into form handlers expecting null.
        this.hide();
        return Promise.resolve(null);
      }

      const actor = browser.browsingContext.currentWindowGlobal.getActor(
        "NRChromeModal",
      );

      const safeForm = JSON.parse(JSON.stringify(form));

      return new Promise((resolve) => {
        let settled = false;
        const settle = (value: TFormResult | null) => {
          if (settled) return;
          settled = true;
          resolve(value);
        };
        // Watchdog: the show query legitimately pends until the user
        // submits the form, so instead probe whether the child page ever
        // mounted. A dead page leaves the overlay blanketing the window
        // with nothing in it ("only the address bar left") — tear it down.
        const watchdog = setTimeout(() => {
          const timeout = new Promise<never>((_, reject) =>
            setTimeout(() => reject(new Error("ping timeout")), 3000)
          );
          Promise.race([actor.sendQuery("NRChromeModal:ping", {}), timeout])
            .then((ready) => {
              if (ready !== true) {
                throw new Error(`child page never mounted (${String(ready)})`);
              }
            })
            .catch((e: unknown) => {
              console.error(
                "[NRChromeModal] modal page unresponsive; removing overlay:",
                e,
              );
              this.hide();
              settle(null);
            });
        }, 8000);
        actor
          .sendQuery("NRChromeModal:show", safeForm)
          .then((response: unknown) => {
            clearTimeout(watchdog);
            settle(response as TFormResult | null);
          })
          .catch((e: unknown) => {
            console.error("[NRChromeModal] show failed:", e);
            clearTimeout(watchdog);
            settle(null);
          });
      });
    }
    return Promise.resolve(null);
  }

  public hide(): void {
    const browser = document?.getElementById(
      "modal-parent-container",
    ) as unknown as XULElement;
    if (browser) {
      setModalVisible(false);
      setModalSize({ width: 600, height: 800 });
      globalThis.focus();
      Services.obs.notifyObservers({}, "nora:modal:hide", "");
    }
  }

  public setModalSize(newSize: ModalSize): void {
    setModalSize((current) => ({ ...current, ...newSize }));
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
