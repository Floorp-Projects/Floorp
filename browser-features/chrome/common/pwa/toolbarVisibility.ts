// SPDX-License-Identifier: MPL-2.0

export function updatePwaToolbarVisibility(
  doc: Document,
  showToolbar: boolean,
): void {
  // Keep the browser's own hidden/collapsed state intact when toggling back on.
  doc.documentElement.toggleAttribute("floorp-pwa-hide-toolbar", !showToolbar);
}
