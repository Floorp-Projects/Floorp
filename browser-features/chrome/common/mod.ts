// SPDX-License-Identifier: MPL-2.0

export function getFeaturesCommonEntries() {
  return import.meta.glob("./*/index.ts");
}
