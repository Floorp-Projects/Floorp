// SPDX-License-Identifier: MPL-2.0

export function getFeaturesStaticEntries() {
  return import.meta.glob("./*/index.ts");
}
