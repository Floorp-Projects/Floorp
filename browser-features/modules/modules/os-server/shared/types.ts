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
 * Cookie data type for get/set cookie operations
 */
export interface CookieData {
  name: string;
  value: string;
  domain?: string;
  path?: string;
  secure?: boolean;
  httpOnly?: boolean;
  sameSite?: "Strict" | "Lax" | "None";
  expirationDate?: number;
}

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
  clickElement(instanceId: string, selector: string): Promise<boolean | null>;
  doubleClick?(instanceId: string, selector: string): Promise<boolean | null>;
  rightClick?(instanceId: string, selector: string): Promise<boolean | null>;
  hoverElement(instanceId: string, selector: string): Promise<boolean | null>;
  scrollToElement(instanceId: string, selector: string): Promise<boolean | null>;
  focusElement?(instanceId: string, selector: string): Promise<boolean | null>;

  // Form operations
  fillForm(
    instanceId: string,
    formData: Record<string, string>,
    options?: { typingMode?: boolean; typingDelayMs?: number },
  ): Promise<boolean | null>;
  inputElement?(
    instanceId: string,
    selector: string,
    value: string,
    options?: { typingMode?: boolean; typingDelayMs?: number },
  ): Promise<boolean | null>;
  clearInput(instanceId: string, selector: string): Promise<boolean | null>;
  submit(instanceId: string, selector: string): Promise<boolean | null>;
  selectOption(instanceId: string, selector: string, value: string): Promise<boolean | null>;
  setChecked(instanceId: string, selector: string, checked: boolean): Promise<boolean | null>;

  // Waiting
  waitForElement(
    instanceId: string,
    selector: string,
    timeout?: number,
    state?: WaitForElementState,
  ): Promise<boolean | null>;
  waitForReady(instanceId: string, timeout?: number): Promise<boolean | null>;
  waitForNetworkIdle?(instanceId: string, timeout?: number): Promise<boolean | null>;

  // Screenshots
  takeScreenshot(instanceId: string): Promise<string | null>;
  takeElementScreenshot(instanceId: string, selector: string): Promise<string | null>;
  takeFullPageScreenshot(instanceId: string): Promise<string | null>;
  takeRegionScreenshot(instanceId: string, rect?: ScreenshotRect): Promise<string | null>;

  // Cookie management
  getCookies(instanceId: string): Promise<CookieData[]>;
  setCookie(instanceId: string, cookie: CookieData): Promise<boolean | null>;

  // Advanced operations
  setInnerHTML(instanceId: string, selector: string, html: string): Promise<boolean | null>;
  setTextContent(instanceId: string, selector: string, text: string): Promise<boolean | null>;
  dispatchEvent(
    instanceId: string,
    selector: string,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): Promise<boolean | null>;
  pressKey?(instanceId: string, key: string): Promise<boolean | null>;
  uploadFile?(instanceId: string, selector: string, filePath: string): Promise<boolean | null>;
  dispatchTextInput(instanceId: string, selector: string, text: string): Promise<boolean | null>;

  // Dialogs
  acceptAlert?(instanceId: string): Promise<boolean | null>;
  dismissAlert?(instanceId: string): Promise<boolean | null>;

  // Drag and drop
  dragAndDrop(instanceId: string, sourceSelector: string, targetSelector: string): Promise<boolean | null>;
}
