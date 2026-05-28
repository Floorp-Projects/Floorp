// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";

export function getJapaneseReadings(id: string): string[] {
  try {
    const readings: unknown = i18next.t(`commandPaletteReadings.${id}`, {
      defaultValue: [] as string[],
      returnObjects: true,
    });
    if (Array.isArray(readings)) {
      return readings.filter((r): r is string => typeof r === "string");
    }
    return [];
  } catch (err) {
    console.error("[CommandPalette] getJapaneseReadings error:", err);
    return [];
  }
}
