/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for WebScraper API
 *
 * WebScraper-specific types that extend the common BrowserAutomationService.
 */

import type { BrowserAutomationService, ScreenshotRect } from "../shared/types.ts";

// Re-export common types
export type { ScreenshotRect };

/**
 * WebScraper API extends the common automation interface with
 * headless-specific instance management.
 */
export interface WebScraperAPI extends BrowserAutomationService {
  // Instance management (headless-specific)
  createInstance(): Promise<string>;
  destroyInstance(instanceId: string): void;
}
