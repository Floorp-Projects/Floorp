/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Parent-process aggregate of editable frames, keyed by the top browser.
 * Frame and browser keys are weak so teardown cannot keep either alive.
 */
export class NRKeyboardShortcutFocusStore {
  private readonly editableFramesByBrowser = new WeakMap<
    object,
    Set<object>
  >();
  private readonly browserByFrame = new WeakMap<object, object>();

  public setFrameEditable(
    browser: object,
    frame: object,
    editable: boolean,
  ): void {
    if (!editable) {
      this.removeFrame(frame);
      return;
    }

    const previousBrowser = this.browserByFrame.get(frame);
    if (previousBrowser && previousBrowser !== browser) {
      this.removeFrame(frame);
    }

    let frames = this.editableFramesByBrowser.get(browser);
    if (!frames) {
      frames = new Set<object>();
      this.editableFramesByBrowser.set(browser, frames);
    }
    frames.add(frame);
    this.browserByFrame.set(frame, browser);
  }

  public removeFrame(frame: object): void {
    const browser = this.browserByFrame.get(frame);
    if (!browser) {
      return;
    }

    const frames = this.editableFramesByBrowser.get(browser);
    frames?.delete(frame);
    if (frames?.size === 0) {
      this.editableFramesByBrowser.delete(browser);
    }
    this.browserByFrame.delete(frame);
  }

  public clearBrowser(browser: object): void {
    const frames = this.editableFramesByBrowser.get(browser);
    if (frames) {
      for (const frame of frames) {
        this.browserByFrame.delete(frame);
      }
    }
    this.editableFramesByBrowser.delete(browser);
  }

  public isEditableFocused(browser: object): boolean {
    return (this.editableFramesByBrowser.get(browser)?.size ?? 0) > 0;
  }
}

export const nrKeyboardShortcutFocusStore = new NRKeyboardShortcutFocusStore();
