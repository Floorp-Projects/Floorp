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
    getActor(name: string): ModalActorLike;
  };
}

export interface ModalActorLike {
  sendQuery(message: string, data: unknown): Promise<unknown>;
}

/** How long to let the page mount before probing whether it is alive. */
export const MODAL_WATCHDOG_DELAY_MS = 8000;
/** How long to wait for an answer to that probe. */
export const MODAL_PING_TIMEOUT_MS = 3000;

/**
 * Ask the child whether its page actually mounted.
 *
 * Resolves false for every unhealthy outcome — the wrong answer, an actor
 * error, or no answer at all within `pingTimeoutMs` — and never rejects, so
 * callers have a single branch to handle. Always clears its own timer,
 * including on the healthy path.
 */
export function probeModalChildAlive(
  actor: ModalActorLike,
  pingTimeoutMs: number = MODAL_PING_TIMEOUT_MS,
): Promise<boolean> {
  return new Promise((resolve) => {
    let done = false;
    const finish = (alive: boolean) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      resolve(alive);
    };

    const timer = setTimeout(() => {
      console.error("[NRChromeModal] health probe timed out");
      finish(false);
    }, pingTimeoutMs);

    try {
      actor
        .sendQuery("NRChromeModal:ping", {})
        .then((ready: unknown) => finish(ready === true))
        .catch((e: unknown) => {
          console.error("[NRChromeModal] health probe failed:", e);
          finish(false);
        });
    } catch (e: unknown) {
      console.error("[NRChromeModal] health probe threw:", e);
      finish(false);
    }
  });
}

/** A modal whose show() promise has not resolved yet. */
interface PendingModal {
  settle: (value: TFormResult | null) => void;
  cancelChild: () => void;
}

export class ModalManager {
  private static get targetParent(): HTMLElement | null {
    return document?.getElementById("main-window") as HTMLElement | null;
  }

  /** Set while a show() promise is outstanding; cleared as it settles. */
  private pending: PendingModal | null = null;

  protected watchdogDelayMs = MODAL_WATCHDOG_DELAY_MS;
  protected pingTimeoutMs = MODAL_PING_TIMEOUT_MS;

  constructor() {
    const handleKeydown = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isModalVisible()) {
        this.hide();
      }
    };
    globalThis.addEventListener("keydown", handleKeydown);
    onCleanup(() => globalThis.removeEventListener("keydown", handleKeydown));
  }

  /** Overridden in tests to supply a stub actor. */
  protected resolveActor(): ModalActorLike | null {
    const browser = document?.getElementById("modal-child-browser") as
      | (XULElement & { browsingContext: BrowsingContextLike })
      | null;
    if (!browser) {
      return null;
    }
    return browser.browsingContext.currentWindowGlobal.getActor("NRChromeModal");
  }

  public show(
    form: TForm,
    options: { width: number; height: number },
  ): Promise<TFormResult | null> {
    const container = document?.getElementById(
      "modal-parent-container",
    ) as unknown as XULElement;
    if (!container) {
      return Promise.resolve(null);
    }

    setModalVisible(true);
    setModalSize({ width: options.width, height: options.height });
    container.focus();

    const actor = this.resolveActor();
    if (!actor) {
      // Never leave callers awaiting undefined — that used to flow an
      // undefined result into form handlers expecting null.
      this.hide();
      return Promise.resolve(null);
    }

    const safeForm = JSON.parse(JSON.stringify(form));

    return new Promise((resolve) => {
      let settled = false;
      let watchdog: ReturnType<typeof setTimeout> | undefined;

      const settle = (value: TFormResult | null) => {
        if (settled) return;
        settled = true;
        if (watchdog !== undefined) {
          clearTimeout(watchdog);
          watchdog = undefined;
        }
        if (this.pending === pending) {
          this.pending = null;
        }
        resolve(value);
      };

      // Release the child from a form that will never be submitted. Best
      // effort by nature: on the dead-page path this is the same actor that
      // is already failing to answer.
      const cancelChild = () => {
        try {
          const query = actor.sendQuery("NRChromeModal:cancel", {});
          query?.catch?.(() => {});
        } catch {
          // Actor already gone; nothing to release.
        }
      };

      // A second show() replaces the first modal's overlay, so release
      // whoever was waiting on it rather than stranding them.
      const replaced = this.pending;
      if (replaced) {
        this.pending = null;
        replaced.cancelChild();
        replaced.settle(null);
      }

      const pending: PendingModal = { settle, cancelChild };
      this.pending = pending;

      // The show query legitimately pends until the user submits, so it
      // cannot itself be timed out. Probe whether the page ever mounted
      // instead: a dead one leaves the overlay blanketing the window with
      // nothing in it ("only the address bar left").
      watchdog = setTimeout(() => {
        probeModalChildAlive(actor, this.pingTimeoutMs).then((alive) => {
          if (alive || settled) return;
          console.error(
            "[NRChromeModal] modal page unresponsive; removing overlay",
          );
          // hide() cancels the child and settles this promise with null.
          this.hide();
        });
      }, this.watchdogDelayMs);

      actor
        .sendQuery("NRChromeModal:show", safeForm)
        .then((response: unknown) => {
          settle(response as TFormResult | null);
        })
        .catch((e: unknown) => {
          console.error("[NRChromeModal] show failed:", e);
          settle(null);
        });
    });
  }

  public hide(): void {
    // Settle first. The overlay can be dismissed without the form ever being
    // submitted — Escape, or the watchdog finding a dead page — and a caller
    // awaiting show() would otherwise wait for a result that is never coming.
    const pending = this.pending;
    this.pending = null;
    if (pending) {
      pending.cancelChild();
      pending.settle(null);
    }

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
