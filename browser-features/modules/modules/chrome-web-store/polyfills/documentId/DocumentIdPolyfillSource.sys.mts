/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Kept for callers that still emit the legacy script path. Native document IDs
 * must reach Gecko unchanged, including callback calls and validation errors.
 */
export const DOCUMENT_ID_POLYFILL_SOURCE =
  "// documentId is provided by the native Floorp extension runtime.\n";
