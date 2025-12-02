/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Represents a detected external browser on the system
 */
export interface ExternalBrowser {
  id: string;
  name: string;
  path: string;
  icon?: string;
  available: boolean;
}

/**
 * Result of attempting to open a URL in an external browser
 */
export interface OpenResult {
  success: boolean;
  error?: string;
}

/**
 * External browser service interface for accessing from chrome components
 */
export interface ExternalBrowserServiceInterface {
  getInstalledBrowsers(forceRefresh?: boolean): Promise<ExternalBrowser[]>;
  openUrl(
    url: string,
    browserId?: string,
    privateMode?: boolean,
  ): Promise<OpenResult>;
  openInDefaultBrowser(url: string): Promise<OpenResult>;
  getBrowser(id: string): Promise<ExternalBrowser | null>;
  clearCache(): void;
}
