// SPDX-License-Identifier: MPL-2.0

import Addons from "../index.ts";
import type { ChromeWebStoreWindow } from "../types.ts";

const timerWindow: Window = window;

/** Real DOM and observer service, with no browser chrome or user window edits. */
export function createAddonWindow(label: string, autoFrames = false) {
  const doc = document.implementation.createHTMLDocument(label);
  Object.defineProperty(doc, "readyState", {
    value: "complete",
    configurable: true,
    writable: true,
  });
  const frames = new Map<number, FrameRequestCallback>();
  const timers = new Map<number, number>();
  let nextFrame = 0;
  const target = Object.assign(new EventTarget(), {
    closed: false,
    document: doc,
    docShell: { itemType: Ci.nsIDocShell.typeChrome },
    requestAnimationFrame(callback: FrameRequestCallback): number {
      const id = ++nextFrame;
      frames.set(id, callback);
      if (autoFrames) {
        timers.set(
          id,
          timerWindow.setTimeout(() => {
            timers.delete(id);
            const frame = frames.get(id);
            frames.delete(id);
            frame?.(performance.now());
          }, 0),
        );
      }
      return id;
    },
    cancelAnimationFrame(id: number): void {
      frames.delete(id);
      const timer = timers.get(id);
      if (timer !== undefined) timerWindow.clearTimeout(timer);
      timers.delete(id);
    },
  });
  const win = target as unknown as
    & ChromeWebStoreWindow
    & Record<string, unknown>;
  return {
    win,
    frames,
    flushFrames(): void {
      const pending = Array.from(frames);
      frames.clear();
      for (const [, callback] of pending) callback(performance.now());
    },
    close(): void {
      if (!target.closed) {
        target.closed = true;
        target.dispatchEvent(new Event("unload"));
      }
      for (const timer of timers.values()) timerWindow.clearTimeout(timer);
      timers.clear();
      frames.clear();
    },
  };
}

/** Exercise the real super()/init() order with an explicit window owner. */
export function createAddonInstance(win: ChromeWebStoreWindow): Addons {
  class TestAddonsOwner extends Addons {
    protected override get ownerWindow(): ChromeWebStoreWindow {
      return win;
    }
  }
  return new TestAddonsOwner(win);
}
