/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  ExternalBrowser,
  ExternalBrowserServiceInterface,
  OpenResult,
} from "./types.ts";

/**
 * Lazy-loaded external browser service that wraps the sys.mts module
 */
class ExternalBrowserServiceWrapper implements ExternalBrowserServiceInterface {
  private _service: ExternalBrowserServiceInterface | null = null;

  private getService(): ExternalBrowserServiceInterface {
    if (!this._service) {
      const { ExternalBrowserService } = ChromeUtils.importESModule(
        "resource://noraneko/modules/external-browser/External-Browser.sys.mjs",
      ) as { ExternalBrowserService: ExternalBrowserServiceInterface };
      this._service = ExternalBrowserService;
    }
    return this._service;
  }

  getInstalledBrowsers(forceRefresh = false): Promise<ExternalBrowser[]> {
    return this.getService().getInstalledBrowsers(forceRefresh);
  }

  openUrl(
    url: string,
    browserId?: string,
    privateMode = false,
  ): Promise<OpenResult> {
    return this.getService().openUrl(url, browserId, privateMode);
  }

  openInDefaultBrowser(url: string): Promise<OpenResult> {
    return this.getService().openInDefaultBrowser(url);
  }

  getBrowser(id: string): Promise<ExternalBrowser | null> {
    return this.getService().getBrowser(id);
  }

  clearCache(): void {
    this.getService().clearCache();
  }
}

/**
 * Singleton instance of the external browser service
 */
export const externalBrowserService = new ExternalBrowserServiceWrapper();
