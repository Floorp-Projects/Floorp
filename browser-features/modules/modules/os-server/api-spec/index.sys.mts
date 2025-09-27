/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * OpenAPI-based type definitions for Floorp OS Local HTTP Server API
 *
 * This replaces the previous Protocol Buffers-based type definitions
 * and provides better compatibility with web standards and tooling.
 */

// Re-export all OpenAPI types
export * from "./types.ts";

// Note: Router types (HttpMethod, RouterContext, RouterHttpResult) are
// imported directly from router.sys.mts in server.sys.mts to avoid duplication
