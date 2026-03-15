/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Valid states for element waiting operations
 */
export type WaitForElementState =
  | "attached" // element exists in DOM
  | "visible" // element is visible
  | "hidden" // element is hidden
  | "detached"; // element is removed from DOM

/**
 * Rectangle for screenshot operations
 */
export type ScreenshotRect = {
  x?: number;
  y?: number;
  width?: number;
  height?: number;
};

/**
 * Common browser automation service interface
 * Implemented by both WebScraper and TabManager services
 */
export interface BrowserAutomationService {
  // Navigation
  navigate(instanceId: string, url: string): Promise<void>;
  getURI(instanceId: string): Promise<string | null>;

  // Content retrieval
  getHTML(instanceId: string): Promise<string | null>;
  getText(instanceId: string): Promise<string | null>;
  getPageTitle(instanceId: string): Promise<string | null>;

  // Element queries
  getElement?(instanceId: string, selector: string): Promise<string | null>;
  getElements(instanceId: string, selector: string): Promise<string[]>;
  getElementText(instanceId: string, selector: string): Promise<string | null>;
  getElementTextContent(
    instanceId: string,
    selector: string,
  ): Promise<string | null>;
  getElementByText(instanceId: string, text: string): Promise<string | null>;

  // Element properties
  getValue(instanceId: string, selector: string): Promise<string | null>;
  getAttribute(
    instanceId: string,
    selector: string,
    name: string,
  ): Promise<string | null>;
  isVisible(instanceId: string, selector: string): Promise<boolean>;
  isEnabled(instanceId: string, selector: string): Promise<boolean>;

  // Element interactions
  clickElement(instanceId: string, selector: string): Promise<boolean>;
  doubleClick?(instanceId: string, selector: string): Promise<boolean>;
  rightClick?(instanceId: string, selector: string): Promise<boolean>;
  hoverElement(instanceId: string, selector: string): Promise<boolean>;
  scrollToElement(instanceId: string, selector: string): Promise<boolean>;
  focusElement?(instanceId: string, selector: string): Promise<boolean>;

  // Form operations
  fillForm(
    instanceId: string,
    formData: Record<string, string>,
    options?: { typingMode?: boolean; typingDelayMs?: number },
  ): Promise<boolean>;
  inputElement?(
    instanceId: string,
    selector: string,
    value: string,
    options?: { typingMode?: boolean; typingDelayMs?: number },
  ): Promise<boolean>;
  clearInput(instanceId: string, selector: string): Promise<boolean>;
  submit(instanceId: string, selector: string): Promise<boolean>;
  selectOption(instanceId: string, selector: string, value: string): Promise<boolean>;
  setChecked(instanceId: string, selector: string, checked: boolean): Promise<boolean>;

  // Waiting
  waitForElement(
    instanceId: string,
    selector: string,
    timeout?: number,
    state?: WaitForElementState,
  ): Promise<boolean>;
  waitForReady(instanceId: string, timeout?: number): Promise<boolean>;
  waitForNetworkIdle?(instanceId: string, timeout?: number): Promise<boolean>;

  // Screenshots
  takeScreenshot(instanceId: string): Promise<string | null>;
  takeElementScreenshot(instanceId: string, selector: string): Promise<string | null>;
  takeFullPageScreenshot(instanceId: string): Promise<string | null>;
  takeRegionScreenshot(instanceId: string, rect?: ScreenshotRect): Promise<string | null>;

  // Cookie management
  getCookies(instanceId: string): Promise<unknown[]>;
  setCookie(instanceId: string, cookie: unknown): Promise<boolean>;

  // Advanced operations
  setInnerHTML(instanceId: string, selector: string, html: string): Promise<boolean>;
  setTextContent(instanceId: string, selector: string, text: string): Promise<boolean>;
  dispatchEvent(
    instanceId: string,
    selector: string,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): Promise<boolean>;
  pressKey?(instanceId: string, key: string): Promise<boolean>;
  uploadFile?(instanceId: string, selector: string, filePath: string): Promise<boolean>;
  dispatchTextInput(instanceId: string, selector: string, text: string): Promise<boolean>;

  // Dialogs
  acceptAlert?(instanceId: string): Promise<boolean>;
  dismissAlert?(instanceId: string): Promise<boolean>;

  // Drag and drop
  dragAndDrop(instanceId: string, sourceSelector: string, targetSelector: string): Promise<boolean>;
}
