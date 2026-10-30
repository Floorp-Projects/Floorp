/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Legacy module entry point. Offscreen documents are now owned by the Runtime,
 * independently of the background page. Never restore the old iframe fallback:
 * it lost documents when an event background stopped and exposed extra APIs.
 */
export interface OffscreenDocumentOptions {
  url: string;
  reasons: OffscreenReason[];
  justification: string;
}

export type OffscreenReason =
  | "TESTING"
  | "AUDIO_PLAYBACK"
  | "IFRAME_SCRIPTING"
  | "DOM_SCRAPING"
  | "BLOBS"
  | "DOM_PARSER"
  | "USER_MEDIA"
  | "DISPLAY_MEDIA"
  | "WEB_RTC"
  | "CLIPBOARD"
  | "LOCAL_STORAGE"
  | "WORKERS"
  | "BATTERY_STATUS"
  | "MATCH_MEDIA"
  | "GEOLOCATION";

export interface OffscreenAPI {
  createDocument(options: OffscreenDocumentOptions): Promise<void>;
  closeDocument(): Promise<void>;
  hasDocument(): Promise<boolean>;
}

function nativeAPI(): OffscreenAPI | undefined {
  const globals = globalThis as unknown as {
    chrome?: { offscreen?: OffscreenAPI };
    browser?: { offscreen?: OffscreenAPI };
  };
  return globals.browser?.offscreen ?? globals.chrome?.offscreen;
}

/** @deprecated Use the native chrome.offscreen API provided by Floorp Runtime. */
export class OffscreenPolyfill implements OffscreenAPI {
  private readonly api: OffscreenAPI | undefined;

  constructor(options?: { nativeAPI?: OffscreenAPI; debug?: boolean }) {
    this.api = options?.nativeAPI ?? nativeAPI();
  }

  private requireAPI(): OffscreenAPI {
    if (!this.api) {
      throw new Error(
        "Offscreen documents require the Floorp compatibility Runtime.",
      );
    }
    return this.api;
  }

  async createDocument(options: OffscreenDocumentOptions): Promise<void> {
    await this.requireAPI().createDocument(options);
  }

  async closeDocument(): Promise<void> {
    await this.requireAPI().closeDocument();
  }

  async hasDocument(): Promise<boolean> {
    return await this.requireAPI().hasDocument();
  }
}

/** @deprecated Kept for old imports; the Runtime installs the native API. */
export function installOffscreenPolyfill(_options?: {
  debug?: boolean;
  force?: boolean;
}): boolean {
  return false;
}
