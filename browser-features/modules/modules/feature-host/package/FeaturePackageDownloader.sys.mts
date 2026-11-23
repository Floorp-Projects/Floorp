// SPDX-License-Identifier: MPL-2.0

import {
  FeaturePackageDownloader,
  FeaturePackageDownloadError,
} from "./FeaturePackageDownloader.ts";
import type {
  FeaturePackageDownloadDiagnostics,
  FeaturePackageDownloadRequest,
} from "./FeaturePackageDownload.types.ts";

const { setTimeout: timerSetTimeout, clearTimeout: timerClearTimeout } =
  ChromeUtils.importESModule("resource://gre/modules/Timer.sys.mjs");
const chromeUtilsWithNow = ChromeUtils as typeof ChromeUtils & {
  now(): number;
};

const downloader = new FeaturePackageDownloader({
  fetch(url, options) {
    return globalThis.fetch(url, options);
  },
  now() {
    return chromeUtilsWithNow.now();
  },
  setTimeout(callback, delayMs) {
    return timerSetTimeout(callback, delayMs);
  },
  clearTimeout(handle) {
    timerClearTimeout(handle);
  },
});

export function downloadFeaturePackageBytes(
  request: FeaturePackageDownloadRequest,
): Promise<Uint8Array> {
  return downloader.downloadFeaturePackageBytes(request);
}

export function featurePackageDownloadDiagnosticsForTests():
  | FeaturePackageDownloadDiagnostics
  | null {
  try {
    return Services.prefs.getStringPref("nora.startup.mode", "") === "test"
      ? downloader.diagnosticsForTests
      : null;
  } catch {
    return null;
  }
}

export { FeaturePackageDownloadError };
export type {
  FeaturePackageDownloadDiagnostics,
  FeaturePackageDownloadErrorCode,
  FeaturePackageDownloadRequest,
} from "./FeaturePackageDownload.types.ts";
