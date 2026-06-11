/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import type { ClickableElementInfo, HintDescriptor } from "./types.ts";

/** Characters used for hint labels, ordered by ease of typing (home row first) */
const DEFAULT_HINT_CHARACTERS = "ASDFJKLQWER";

export class HintGenerator {
  private characters: string;

  constructor(characters: string = DEFAULT_HINT_CHARACTERS) {
    this.characters = characters;
  }

  /**
   * Generate hint descriptors for the given clickable elements.
   * Uses the AlphabetHints algorithm:
   * 1. Build prefix combinations until enough unique hints exist
   * 2. Sort alphabetically, then reverse each string
   * 3. Assign to elements in viewport order
   */
  generate(elements: ClickableElementInfo[]): HintDescriptor[] {
    if (elements.length === 0) return [];

    const labels = this.generateLabels(elements.length);

    return elements.map((el, index) => ({
      label: labels[index],
      elementIndex: index,
      rect: el.rect,
    }));
  }

  private generateLabels(count: number): string[] {
    const chars = this.characters;
    if (chars.length === 0) return [];

    // Build hints using prefix combinations
    const hints: string[] = [""];
    let offset = 0;
    // Use do…while to ensure at least one round of generation, which avoids
    // producing an empty-string label when count is small (e.g. 1).
    do {
      const hint = hints[offset];
      offset++;
      for (let i = 0; i < chars.length; i++) {
        hints.push(chars[i] + hint);
      }
    } while (hints.length - offset < count);

    // Trim to exact count
    const result = hints.slice(offset, offset + count);

    // Sort alphabetically then reverse each string
    result.sort();
    for (let i = 0; i < result.length; i++) {
      result[i] = result[i].split("").reverse().join("");
    }

    return result;
  }
}
