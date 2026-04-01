// SPDX-License-Identifier: MPL-2.0

// This file was previously the browser-side test entry point.
// Test discovery and execution is now handled by:
//   bridge/loader-features/loader/test/index.ts
//
// That entry is loaded by chrome_root.ts when MODE === "test"
// and discovers tests via import.meta.glob with structured result reporting.
