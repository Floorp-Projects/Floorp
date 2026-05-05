// SPDX-License-Identifier: MPL-2.0
// Stage 0 stub: preact-xul build environment verification
// This file verifies that the preact + @preact/signals toolchain is correctly
// configured before migrating the actual feature files.

import { h, Fragment } from "@nora/preact-xul";
import { signal, effect, computed } from "@preact/signals";

// Verify signal primitives resolve
const _count = signal(0);
const _double = computed(() => _count.value * 2);
const _dispose = effect(() => {
  void _double.value;
});
_dispose();

// Verify JSX transform (this line checks jsxImportSource = "preact")
const _elem = h("div", null, "Stage 0 OK");
void _elem;
void Fragment;

console.log("[noraneko-stub] Stage 0: preact-xul build environment OK");

export default async function initScripts() {
  console.log("[noraneko-stub] stub initScripts — Stage 0 placeholder");
}
