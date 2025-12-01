/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared type definitions for browser automation services (Scraper & Tab)
 */

export interface ScreenshotRect {
  x?: number;
  y?: number;
  width?: number;
  height?: number;
}

/**
 * Common interface for browser automation services.
 * Both WebScraper and TabManager implement these methods.
 */
export interface BrowserAutomationService {
  // Navigation
  navigate(instanceId: string, url: string): Promise<void>;
  getURI(instanceId: string): Promise<string | null>;
  getHTML(instanceId: string): Promise<string | null>;

  // Element access
  getElement?(instanceId: string, selector: string): Promise<string | null>;
  getElements(instanceId: string, selector: string): Promise<string[]>;
  getElementByText(
    instanceId: string,
    textContent: string,
  ): Promise<string | null>;
  getElementTextContent(
    instanceId: string,
    selector: string,
  ): Promise<string | null>;
  getElementText(instanceId: string, selector: string): Promise<string | null>;

  // Interaction
  clickElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null>;
  waitForElement(
    instanceId: string,
    selector: string,
    timeout?: number,
  ): Promise<boolean | null>;

  // Screenshots
  takeScreenshot(instanceId: string): Promise<string | null>;
  takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null>;
  takeFullPageScreenshot(instanceId: string): Promise<string | null>;
  takeRegionScreenshot(
    instanceId: string,
    rect?: ScreenshotRect,
  ): Promise<string | null>;

  // Forms
  fillForm(
    instanceId: string,
    formData: { [selector: string]: string },
  ): Promise<boolean | null>;
  getValue(instanceId: string, selector: string): Promise<string | null>;
  submit(instanceId: string, selector: string): Promise<boolean | null>;
}

