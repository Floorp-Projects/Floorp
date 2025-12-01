/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for TabManager API
 *
 * TabManager-specific types that extend the common BrowserAutomationService.
 */

import type { BrowserAutomationService, ScreenshotRect } from "../shared/types.ts";

// Re-export common types
export type { ScreenshotRect };

export interface TabInfo {
  browserId: string;
  title: string;
  url: string;
  selected: boolean;
  pinned: boolean;
}

/**
 * TabManager API extends the common automation interface with
 * visible tab management capabilities.
 */
export interface TabManagerAPI extends BrowserAutomationService {
  // Instance management (tab-specific)
  createInstance(
    url: string,
    options?: { inBackground?: boolean },
  ): Promise<string>;
  attachToTab(browserId: string): Promise<string | null>;
  listTabs(): Promise<TabInfo[]>;
  getInstanceInfo(instanceId: string): Promise<unknown | null>;
  destroyInstance(instanceId: string): Promise<void>;

  // TabManager also has getElement
  getElement(instanceId: string, selector: string): Promise<string | null>;

  // Override getURI to return non-null (tabs always have a URI)
  getURI(instanceId: string): Promise<string>;
}
