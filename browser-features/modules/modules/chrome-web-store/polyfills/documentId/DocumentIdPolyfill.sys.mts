/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Compatibility entry point for callers of the former documentId polyfill.
 * The pinned Gecko runtime maps document UUIDs to live inner windows and rejects
 * stale targets. Translating those UUIDs to frame IDs bypasses that protection.
 */
export interface DocumentIdPolyfillOptions {
  debug?: boolean;
  force?: boolean;
}

/** @deprecated documentId is provided by the native extension APIs. */
export function installDocumentIdPolyfill(
  _options?: DocumentIdPolyfillOptions,
): boolean {
  return false;
}
