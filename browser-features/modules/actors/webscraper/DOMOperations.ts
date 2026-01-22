/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DOMOperations - facade that delegates to smaller operation modules
 */

import type {
  InputElementOptions,
  SelectOptionOptions,
  WebScraperContext,
} from "./types.ts";
import { HighlightManager } from "./HighlightManager.ts";
import { EventDispatcher } from "./EventDispatcher.ts";
import { TranslationHelper } from "./TranslationHelper.ts";
import type { DOMOpsDeps } from "./DOMDeps.ts";
import { DOMReadOperations } from "./DOMReadOperations.ts";
import { DOMWriteOperations } from "./DOMWriteOperations.ts";
import { DOMActionOperations } from "./DOMActionOperations.ts";
import { DOMWaitOperations } from "./DOMWaitOperations.ts";

export class DOMOperations {
  private highlightManager: HighlightManager;
  private eventDispatcher: EventDispatcher;
  private translationHelper: TranslationHelper;
  private readOps: DOMReadOperations;
  private writeOps: DOMWriteOperations;
  private actionOps: DOMActionOperations;
  private waitOps: DOMWaitOperations;

  constructor(private context: WebScraperContext) {
    this.highlightManager = new HighlightManager(context);
    this.eventDispatcher = new EventDispatcher(context);
    this.translationHelper = new TranslationHelper(context);

    const deps: DOMOpsDeps = {
      context,
      highlightManager: this.highlightManager,
      eventDispatcher: this.eventDispatcher,
      translationHelper: this.translationHelper,
      getContentWindow: () => this.contentWindow,
      getDocument: () => this.document,
    };

    this.readOps = new DOMReadOperations(deps);
    this.writeOps = new DOMWriteOperations(deps);
    this.actionOps = new DOMActionOperations(deps);
    this.waitOps = new DOMWaitOperations(deps);
  }

  get contentWindow(): (Window & typeof globalThis) | null {
    return this.context.contentWindow;
  }

  get document(): Document | null {
    return this.context.document;
  }

  getHighlightManager(): HighlightManager {
    return this.highlightManager;
  }

  getEventDispatcher(): EventDispatcher {
    return this.eventDispatcher;
  }

  getTranslationHelper(): TranslationHelper {
    return this.translationHelper;
  }

  // Read ops
  getHTML(): string | null {
    return this.readOps.getHTML();
  }

  getElement(selector: string): string | null {
    return this.readOps.getElement(selector);
  }

  getElements(selector: string): string[] {
    return this.readOps.getElements(selector);
  }

  getElementByText(textContent: string): string | null {
    return this.readOps.getElementByText(textContent);
  }

  getElementText(selector: string): string | null {
    return this.readOps.getElementText(selector);
  }

  getElementTextContent(selector: string): string | null {
    return this.readOps.getElementTextContent(selector);
  }

  getValue(selector: string): Promise<string | null> {
    return this.readOps.getValue(selector);
  }

  getAttribute(selector: string, attributeName: string): string | null {
    return this.readOps.getAttribute(selector, attributeName);
  }

  isVisible(selector: string): boolean {
    return this.readOps.isVisible(selector);
  }

  isEnabled(selector: string): boolean {
    return this.readOps.isEnabled(selector);
  }

  getPageTitle(): string | null {
    return this.readOps.getPageTitle();
  }

  // Write/input ops
  inputElement(
    selector: string,
    value: string,
    options: InputElementOptions = {},
  ): Promise<boolean> {
    return this.writeOps.inputElement(selector, value, options);
  }

  clearInput(selector: string): Promise<boolean> {
    return this.writeOps.clearInput(selector);
  }

  selectOption(
    selector: string,
    value: string,
    opts: SelectOptionOptions = {},
  ): Promise<boolean> {
    return this.writeOps.selectOption(selector, value, opts);
  }

  setChecked(selector: string, checked: boolean): Promise<boolean> {
    return this.writeOps.setChecked(selector, checked);
  }

  uploadFile(selector: string, filePath: string): Promise<boolean> {
    return this.writeOps.uploadFile(selector, filePath);
  }

  setCookieString(
    cookieString: string,
    cookieName?: string,
    cookieValue?: string,
  ): boolean {
    return this.writeOps.setCookieString(cookieString, cookieName, cookieValue);
  }

  setInnerHTML(selector: string, html: string): Promise<boolean> {
    return this.writeOps.setInnerHTML(selector, html);
  }

  setTextContent(selector: string, text: string): Promise<boolean> {
    return this.writeOps.setTextContent(selector, text);
  }

  // Interaction ops
  clickElement(selector: string): Promise<boolean> {
    return this.actionOps.clickElement(selector);
  }

  hoverElement(selector: string): Promise<boolean> {
    return this.actionOps.hoverElement(selector);
  }

  scrollToElement(selector: string): Promise<boolean> {
    return this.actionOps.scrollToElement(selector);
  }

  doubleClickElement(selector: string): Promise<boolean> {
    return this.actionOps.doubleClickElement(selector);
  }

  rightClickElement(selector: string): Promise<boolean> {
    return this.actionOps.rightClickElement(selector);
  }

  focusElement(selector: string): Promise<boolean> {
    return this.actionOps.focusElement(selector);
  }

  pressKey(keyCombo: string): Promise<boolean> {
    return this.actionOps.pressKey(keyCombo);
  }

  dragAndDrop(
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean> {
    return this.actionOps.dragAndDrop(sourceSelector, targetSelector);
  }

  // Wait ops
  waitForElement(
    selector: string,
    timeout = 5000,
    signal?: AbortSignal,
  ): Promise<boolean> {
    return this.waitOps.waitForElement(selector, timeout, signal);
  }

  waitForReady(timeout = 15000, signal?: AbortSignal): Promise<boolean> {
    return this.waitOps.waitForReady(timeout, signal);
  }

  dispatchEvent(
    selector: string,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): boolean {
    try {
      const doc = this.document;
      if (!doc) {
        return false;
      }

      const element = doc.querySelector(selector);
      if (!element) {
        console.warn(
          `DOMOperations: Element not found for dispatchEvent: ${selector}`,
        );
        return false;
      }

      return this.eventDispatcher.dispatchCustomEvent(
        element,
        eventType,
        options,
      );
    } catch (e) {
      console.error("DOMOperations: Error in dispatchEvent:", e);
      return false;
    }
  }

  destroy(): void {
    this.highlightManager.destroy();
  }
}
