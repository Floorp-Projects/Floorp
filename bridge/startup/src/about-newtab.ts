// SPDX-License-Identifier: MPL-2.0

// @ts-expect-error: chrome:// URL not resolvable by TypeScript
import("chrome://noraneko/skin/newtab.script.js").catch((e) =>
  console.error("[noraneko] Failed to load newtab script:", e),
);
