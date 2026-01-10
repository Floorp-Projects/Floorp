/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRWebScraperChild - Content process actor for web scraping operations
 *
 * This actor runs in the content process and provides functionality to:
 * - Extract HTML content from web pages
 * - Interact with DOM elements (input fields, textareas)
 * - Handle messages from the parent process WebScraper service
 * - Provide safe access to content window and document objects
 *
 * The actor is automatically created for each browser tab/content window
 * and communicates with the parent process through message passing.
 */

import type {
  NRWebScraperMessageData,
  WebScraperContext,
} from "./webscraper/types.ts";
import { DOMOperations } from "./webscraper/DOMOperations.ts";
import { FormOperations } from "./webscraper/FormOperations.ts";
import { ScreenshotOperations } from "./webscraper/ScreenshotOperations.ts";

export class NRWebScraperChild extends JSWindowActorChild {
  private domOps: DOMOperations | null = null;
  private formOps: FormOperations | null = null;
  private screenshotOps: ScreenshotOperations | null = null;
  private pageHideHandler: (() => void) | null = null;

  /**
   * Lazily create and return the context object
   */
  private getContext(): WebScraperContext {
    return {
      contentWindow: this.contentWindow as
        | (Window & typeof globalThis)
        | null,
      document: this.document,
      sendQuery: (name: string, data?: unknown) =>
        this.sendQuery(name, data),
    };
  }

  /**
   * Get DOMOperations instance (lazy initialization)
   */
  private getDOMOps(): DOMOperations {
    if (!this.domOps) {
      this.domOps = new DOMOperations(this.getContext());
    }
    return this.domOps;
  }

  /**
   * Get FormOperations instance (lazy initialization)
   */
  private getFormOps(): FormOperations {
    if (!this.formOps) {
      this.formOps = new FormOperations(this.getContext());
    }
    return this.formOps;
  }

  /**
   * Get ScreenshotOperations instance (lazy initialization)
   */
  private getScreenshotOps(): ScreenshotOperations {
    if (!this.screenshotOps) {
      this.screenshotOps = new ScreenshotOperations(this.getContext());
    }
    return this.screenshotOps;
  }

  /**
   * Get HighlightManager from DOMOperations
   */
  private getHighlightManager() {
    return this.getDOMOps().getHighlightManager();
  }

  /**
   * Handles DOM events derived from JSWindowActor registration.
   * Required to prevent "Property 'handleEvent' is not callable" errors.
   */
  handleEvent(_event: Event): void {
    // No-op: We only listen to trigger actor creation or specific side-effects
  }

  /**
   * Called when the actor is created for a content window
   */
  actorCreated() {
    console.log(
      "NRWebScraperChild created for:",
      this.contentWindow?.location?.href,
    );

    // SPAナビゲーション対応: pagehideイベントでクリーンアップ
    const win = this.contentWindow;
    if (win) {
      this.pageHideHandler = () => {
        this.getHighlightManager().cleanupHighlight();
        this.getHighlightManager().hideInfoPanel();
      };
      win.addEventListener("pagehide", this.pageHideHandler);
    }
  }

  /**
   * Called when the actor is about to be destroyed
   */
  willDestroy() {
    // pagehideイベントリスナーを解除
    const win = this.contentWindow;
    if (win && this.pageHideHandler) {
      try {
        win.removeEventListener("pagehide", this.pageHideHandler);
      } catch {
        // DeadObject - 無視
      }
    }
    this.pageHideHandler = null;

    // Clean up all operations
    if (this.domOps) {
      this.domOps.destroy();
      this.domOps = null;
    }
    if (this.formOps) {
      this.formOps.destroy();
      this.formOps = null;
    }
    this.screenshotOps = null;
  }

  /**
   * Handles incoming messages from the parent process
   */
  receiveMessage(message: { name: string; data?: NRWebScraperMessageData }) {
    const domOps = this.getDOMOps();
    const formOps = this.getFormOps();
    const screenshotOps = this.getScreenshotOps();
    const highlightManager = this.getHighlightManager();

    switch (message.name) {
      case "WebScraper:WaitForReady": {
        const to = message.data?.timeout || 15000;
        return domOps.waitForReady(to);
      }
      case "WebScraper:GetHTML":
        return domOps.getHTML();
      case "WebScraper:GetElements":
        if (message.data?.selector) {
          return domOps.getElements(message.data.selector);
        }
        break;
      case "WebScraper:GetElementByText":
        if (message.data?.textContent) {
          return domOps.getElementByText(message.data.textContent);
        }
        break;
      case "WebScraper:GetElementTextContent":
        if (message.data?.selector) {
          return domOps.getElementTextContent(message.data.selector);
        }
        break;
      case "WebScraper:GetElement":
        if (message.data?.selector) {
          return domOps.getElement(message.data.selector);
        }
        break;
      case "WebScraper:GetElementText":
        if (message.data?.selector) {
          return domOps.getElementText(message.data.selector);
        }
        break;
      case "WebScraper:GetValue":
        if (message.data?.selector) {
          return domOps.getValue(message.data.selector);
        }
        break;
      case "WebScraper:InputElement":
        if (message.data?.selector && typeof message.data.value === "string") {
          return domOps.inputElement(message.data.selector, message.data.value, {
            typingMode: message.data.typingMode,
            typingDelayMs: message.data.typingDelayMs,
          });
        }
        break;
      case "WebScraper:ClickElement":
        if (message.data?.selector) {
          return domOps.clickElement(message.data.selector);
        }
        break;
      case "WebScraper:WaitForElement":
        if (message.data?.selector) {
          return domOps.waitForElement(
            message.data.selector,
            message.data.timeout || 5000,
          );
        }
        break;
      case "WebScraper:TakeScreenshot":
        return screenshotOps.takeScreenshot();
      case "WebScraper:TakeElementScreenshot":
        if (message.data?.selector) {
          return screenshotOps.takeElementScreenshot(message.data.selector);
        }
        break;
      case "WebScraper:TakeFullPageScreenshot":
        return screenshotOps.takeFullPageScreenshot();
      case "WebScraper:TakeRegionScreenshot":
        if (message.data) {
          return screenshotOps.takeRegionScreenshot(message.data.rect || {});
        }
        break;
      case "WebScraper:FillForm":
        if (message.data?.formData) {
          return formOps.fillForm(message.data.formData, {
            typingMode: message.data.typingMode,
            typingDelayMs: message.data.typingDelayMs,
          });
        }
        break;
      case "WebScraper:Submit":
        if (message.data?.selector) {
          return formOps.submit(message.data.selector);
        }
        break;
      case "WebScraper:ClearEffects":
        highlightManager.cleanupHighlight();
        highlightManager.hideInfoPanel();
        highlightManager.hideControlOverlay();
        return true;
      case "WebScraper:ShowControlOverlay":
        highlightManager.showControlOverlay();
        return true;
      case "WebScraper:HideControlOverlay":
        highlightManager.hideControlOverlay();
        return true;
      case "WebScraper:GetAttribute":
        if (message.data?.selector && message.data?.attributeName) {
          return domOps.getAttribute(
            message.data.selector,
            message.data.attributeName,
          );
        }
        break;
      case "WebScraper:IsVisible":
        if (message.data?.selector) {
          return domOps.isVisible(message.data.selector);
        }
        break;
      case "WebScraper:IsEnabled":
        if (message.data?.selector) {
          return domOps.isEnabled(message.data.selector);
        }
        break;
      case "WebScraper:ClearInput":
        if (message.data?.selector) {
          return domOps.clearInput(message.data.selector);
        }
        break;
      case "WebScraper:SelectOption":
        if (message.data?.selector && message.data?.optionValue !== undefined) {
          return domOps.selectOption(
            message.data.selector,
            message.data.optionValue,
          );
        }
        break;
      case "WebScraper:SetChecked":
        if (
          message.data?.selector &&
          typeof message.data?.checked === "boolean"
        ) {
          return domOps.setChecked(message.data.selector, message.data.checked);
        }
        break;
      case "WebScraper:HoverElement":
        if (message.data?.selector) {
          return domOps.hoverElement(message.data.selector);
        }
        break;
      case "WebScraper:ScrollToElement":
        if (message.data?.selector) {
          return domOps.scrollToElement(message.data.selector);
        }
        break;
      case "WebScraper:DoubleClick":
        if (message.data?.selector) {
          return domOps.doubleClickElement(message.data.selector);
        }
        break;
      case "WebScraper:RightClick":
        if (message.data?.selector) {
          return domOps.rightClickElement(message.data.selector);
        }
        break;
      case "WebScraper:Focus":
        if (message.data?.selector) {
          return domOps.focusElement(message.data.selector);
        }
        break;
      case "WebScraper:GetPageTitle":
        return domOps.getPageTitle();
      case "WebScraper:DragAndDrop":
        if (message.data?.selector && message.data?.targetSelector) {
          return domOps.dragAndDrop(
            message.data.selector,
            message.data.targetSelector,
          );
        }
        break;
      case "WebScraper:SetInnerHTML":
        if (
          message.data?.selector &&
          typeof message.data?.innerHTML === "string"
        ) {
          return domOps.setInnerHTML(
            message.data.selector,
            message.data.innerHTML,
          );
        }
        break;
      case "WebScraper:SetTextContent":
        if (
          message.data?.selector &&
          typeof message.data?.textContent === "string"
        ) {
          return domOps.setTextContent(
            message.data.selector,
            message.data.textContent,
          );
        }
        break;
      case "WebScraper:DispatchEvent":
        if (message.data?.selector && message.data?.eventType) {
          return domOps.dispatchEvent(
            message.data.selector,
            message.data.eventType,
            message.data.eventOptions,
          );
        }
        break;
      case "WebScraper:PressKey":
        if (message.data?.key) {
          return domOps.pressKey(message.data.key);
        }
        break;
      case "WebScraper:UploadFile":
        if (message.data?.selector && message.data?.filePath) {
          return domOps.uploadFile(message.data.selector, message.data.filePath);
        }
        break;
      case "WebScraper:SetCookieString":
        if (message.data?.cookieString) {
          return domOps.setCookieString(
            message.data.cookieString,
            message.data.cookieName,
            message.data.cookieValue,
          );
        }
        break;
    }
    return null;
  }
}
