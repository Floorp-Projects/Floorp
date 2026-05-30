// SPDX-License-Identifier: MPL-2.0

/**
 * Loader entry point — backward-compatible re-export shim.
 *
 * External consumers (chrome_root.ts, vite.config.ts) reference this file
 * directly. It delegates everything to mod.ts, which is the canonical
 * DOP-structured entry point.
 *
 * Do not add logic here; put it in the appropriate DOP layer instead.
 */

export { default } from "./mod.ts";
export * from "./mod.ts";
