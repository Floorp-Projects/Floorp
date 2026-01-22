/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";

const { setTimeout: timerSetTimeout, clearTimeout: timerClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

/**
 * Wait helpers (element readiness/document readiness)
 */
export class DOMWaitOperations {
  constructor(private deps: DOMOpsDeps) {}

  private get contentWindow(): (Window & typeof globalThis) | null {
    return this.deps.getContentWindow();
  }

  private get document(): Document | null {
    return this.deps.getDocument();
  }

  async waitForElement(
    selector: string,
    timeout = 5000,
    signal?: AbortSignal,
  ): Promise<boolean> {
    const doc = this.document;
    if (!doc) return false;

    if (signal?.aborted) return false;

    return await new Promise<boolean>((resolve) => {
      let finished = false;

      const finish = (result: boolean) => {
        if (finished) return;
        finished = true;
        try {
          observer?.disconnect();
        } catch {
          // ignore
        }
        if (timeoutId !== null) {
          timerClearTimeout(timeoutId);
        }
        if (signal && abortHandler) {
          signal.removeEventListener("abort", abortHandler);
        }
        resolve(result);
      };

      const abortHandler = () => finish(false);
      if (signal) {
        signal.addEventListener("abort", abortHandler, { once: true });
      }

      const safeCheck = () => {
        try {
          const found = doc.querySelector(selector);
          if (found) {
            finish(true);
          }
        } catch (e) {
          if (
            e &&
            typeof e === "object" &&
            "message" in e &&
            typeof (e as { message?: unknown }).message === "string" &&
            ((e as { message: string }).message?.includes("dead object") ??
              false)
          ) {
            finish(false);
            return;
          }
          console.error("DOMWaitOperations: Error waiting for element:", e);
          finish(false);
        }
      };

      const win = this.contentWindow;
      const MutationObs = win?.MutationObserver ?? globalThis.MutationObserver;
      let observer: MutationObserver | null = null;

      const timeoutId = Number(
        timerSetTimeout(() => finish(false), Math.max(0, timeout)),
      );

      // Initial check before observing
      safeCheck();
      if (finished) return;

      if (MutationObs) {
        try {
          observer = new MutationObs(() => safeCheck());
          observer.observe(doc, { childList: true, subtree: true });
        } catch {
          // Fallback: final timeout will resolve
        }
      }
    });
  }

  async waitForReady(timeout = 15000, signal?: AbortSignal): Promise<boolean> {
    const win = this.contentWindow;
    if (!win) return false;
    const doc = win.document;
    if (!doc) return false;

    return await new Promise<boolean>((resolve) => {
      let finished = false;

      const finish = (result: boolean) => {
        if (finished) return;
        finished = true;
        if (timeoutId !== null) timerClearTimeout(timeoutId);
        try {
          doc.removeEventListener("readystatechange", checkReady, true);
        } catch {
          // ignore
        }
        try {
          win.removeEventListener("DOMContentLoaded", checkReady, true);
        } catch {
          // ignore
        }
        if (signal && abortHandler) {
          signal.removeEventListener("abort", abortHandler);
        }
        resolve(result);
      };

      const abortHandler = () => finish(false);
      if (signal) {
        if (signal.aborted) {
          finish(false);
          return;
        }
        signal.addEventListener("abort", abortHandler, { once: true });
      }

      const checkReady = () => {
        try {
          if (
            doc.documentElement &&
            (doc.body ||
              doc.readyState === "interactive" ||
              doc.readyState === "complete")
          ) {
            finish(true);
          }
        } catch (e) {
          if (
            e &&
            typeof e === "object" &&
            "message" in e &&
            typeof (e as { message?: unknown }).message === "string" &&
            ((e as { message: string }).message?.includes("dead object") ??
              false)
          ) {
            finish(false);
            return;
          }
          finish(false);
        }
      };

      const timeoutId = Number(
        timerSetTimeout(() => finish(false), Math.max(0, timeout)),
      );

      checkReady();
      if (finished) return;

      try {
        doc.addEventListener("readystatechange", checkReady, true);
        win.addEventListener("DOMContentLoaded", checkReady, true);
      } catch {
        // Fallback: rely on timeout
      }
    });
  }
}
