// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import { JAPANESE_COMMAND_READINGS } from "./japaneseReadings.ts";

/**
 * Returns possible Japanese readings for a given command identifier.
 * Readings live outside translated resources so synchronization cannot erase them.
 * @param id - The command identifier to look up readings for.
 * @returns An array of Japanese reading strings, or an empty array if none found.
 */
export function getJapaneseReadings(id: string): string[] {
  if (
    !/^ja(?:-|$)/i.test(i18next.language ?? "") ||
    !Object.hasOwn(JAPANESE_COMMAND_READINGS, id)
  ) {
    return [];
  }
  return [...JAPANESE_COMMAND_READINGS[id]];
}
