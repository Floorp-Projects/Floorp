/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DOMOpsDeps } from "./DOMDeps.ts";

const { setTimeout: timerSetTimeout, clearTimeout: timerClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");

export type WaitForElementState =
  | "attached"
  | "visible"
  | "hidden"
  | "detached";

/**
 * Wait helpers (element readiness/document readiness)
 */
export class DOMWaitOperations {
  constructor(private deps: DOMOpsDeps) {}

  /**
   * Check if an error is a "dead object" error (occurs when document is navigated away)
   */
  private isDeadObjectError(e: unknown): boolean {
    if (!e || typeof e !== "object") return false;
    const message = (e as { message?: unknown }).message;
    return typeof message === "string" && message.includes("dead object");
  }

  private get contentWindow(): (Window & typeof globalThis) | null {
    return this.deps.getContentWindow();
  }

  private get document(): Document | null {
    return this.deps.getDocument();
  }

  /**
   * Check if an element is visible (not hidden by CSS)
   */
  private isVisible(element: Element | null): boolean {
    if (!element) return false;

    const win = this.contentWindow;
    if (!win) return false;

    try {
      const style = win.getComputedStyle(element);
      const rect = element.getBoundingClientRect();

      return (
        style.display !== "none" &&
        style.visibility !== "hidden" &&
        style.opacity !== "0" &&
        rect.width > 0 &&
        rect.height > 0
      );
    } catch {
      return false;
    }
  }

  /**
   * Check if element matches the desired state
   */
  private matchesState(
    element: Element | null,
    state: WaitForElementState,
  ): boolean {
    switch (state) {
      case "attached":
        return !!element;
      case "visible":
        return this.isVisible(element);
      case "hidden":
        return !element || !this.isVisible(element);
      case "detached":
        return !element;
      default:
        return !!element;
    }
  }

  async waitForElement(
    selector: string,
    timeout = 5000,
    signal?: AbortSignal,
    state: WaitForElementState = "attached",
  ): Promise<boolean> {
    const doc = this.document;
    if (!doc) return false;

    if (signal?.aborted) return false;

    // For detached state, we need to wait for element to be removed
    if (state === "detached") {
      return this.waitForDetached(selector, timeout, signal);
    }

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
          if (this.matchesState(found, state)) {
            finish(true);
          }
        } catch (e) {
          if (this.isDeadObjectError(e)) {
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

  /**
   * Wait for an element to be detached from the DOM
   */
  private async waitForDetached(
    selector: string,
    timeout: number,
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
          if (!found) {
            finish(true);
          }
        } catch (e) {
          if (this.isDeadObjectError(e)) {
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
          if (this.isDeadObjectError(e)) {
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
