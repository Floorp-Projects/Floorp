/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * HTTP-related type definitions for the OS Server
 */

export type HeadersMap = Record<string, string>;

export interface HttpRequest {
  method: string;
  path: string; // includes query string
  headers: HeadersMap;
  body: Uint8Array;
}
