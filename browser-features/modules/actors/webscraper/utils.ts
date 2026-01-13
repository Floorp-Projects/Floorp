/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared Xray unwrap helpers used across webscraper modules
 */

import type { ContentWindow, RawContentWindow } from "./types.ts";

/**
 * Helper to unwrap Xray-wrapped window
 */
export function unwrapWindow(
  win: ContentWindow | null,
): RawContentWindow | null {
  if (!win) return null;
  return win.wrappedJSObject ?? (win as unknown as RawContentWindow);
}

/**
 * Helper to unwrap Xray-wrapped element
 */
export function unwrapElement<T extends Element>(
  element: T & Partial<{ wrappedJSObject: T }>,
): T {
  return element.wrappedJSObject ?? element;
}

/**
 * Helper to unwrap Xray-wrapped document
 */
export function unwrapDocument(
  doc: Document & Partial<{ wrappedJSObject: Document }>,
): Document {
  return doc.wrappedJSObject ?? doc;
}
