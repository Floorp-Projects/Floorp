/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import type { HintDescriptor } from "./types.ts";

const HINT_CONTAINER_ID = "nr-linkhints-container";
const HINT_STYLE_ID = "nr-linkhints-style";

const HINT_STYLES = `
  #${HINT_CONTAINER_ID} {
    position: fixed;
    top: 0;
    left: 0;
    width: 0;
    height: 0;
    z-index: 2147483647;
    pointer-events: none;
  }
  .nr-linkhints-hint {
    position: fixed;
    padding: 1px 4px;
    font-family: monospace;
    font-size: 11px;
    font-weight: bold;
    color: #000;
    background-color: #FFD700;
    border: 1px solid #B8860B;
    border-radius: 2px;
    pointer-events: none;
    white-space: nowrap;
    transition: opacity 0.1s ease;
  }
  .nr-linkhints-hint--matched {
    background-color: #90EE90;
    border-color: #228B22;
  }
  .nr-linkhints-hint--hidden {
    display: none;
  }
  .nr-linkhints-hint .nr-linkhints-char {
    opacity: 1;
  }
  .nr-linkhints-hint .nr-linkhints-char--dimmed {
    opacity: 0.3;
  }
`;

/** Fixed pixel height of a hint marker, used for positioning above elements. */
const HINT_MARKER_HEIGHT = 18;

export class HintOverlay {
  private container: HTMLDivElement | null = null;
  private styleElement: HTMLStyleElement | null = null;
  private hintElements: HTMLSpanElement[] = [];

  /**
   * Create and display hint markers for the given descriptors.
   *
   * Hints are positioned at activation time and do not track scroll/resize.
   * A future iteration can add debounced re-detection if needed.
   */
  show(descriptors: HintDescriptor[], doc: Document): void {
    this.cleanup(doc);

    // Inject styles
    const existingStyle = doc.getElementById(HINT_STYLE_ID);
    if (!existingStyle) {
      const style = doc.createElement("style");
      style.id = HINT_STYLE_ID;
      style.textContent = HINT_STYLES;
      (doc.head ?? doc.documentElement)?.append(style);
      this.styleElement = style;
    }

    // Create container
    const container = doc.createElement("div");
    container.id = HINT_CONTAINER_ID;
    (doc.body ?? doc.documentElement)?.append(container);
    this.container = container;

    // Create hint markers
    for (const desc of descriptors) {
      const marker = doc.createElement("span");
      marker.className = "nr-linkhints-hint";
      marker.dataset.label = desc.label;
      marker.dataset.elementIndex = String(desc.elementIndex);

      // Create individual character spans for highlighting
      for (const char of desc.label) {
        const charSpan = doc.createElement("span");
        charSpan.className = "nr-linkhints-char";
        charSpan.textContent = char;
        marker.appendChild(charSpan);
      }

      container.appendChild(marker);
      this.positionMarker(marker, desc.rect);
      this.hintElements.push(marker);
    }
  }

  /**
   * Filter visible hints based on typed characters.
   * @param typedChars - Characters typed so far (lowercase)
   * @returns Array of element indices that match the filter
   */
  filter(typedChars: string): number[] {
    const matchedIndices: number[] = [];
    const typedLower = typedChars.toLowerCase();

    for (const marker of this.hintElements) {
      const label = (marker.dataset.label ?? "").toLowerCase();
      if (label.startsWith(typedLower)) {
        marker.classList.remove("nr-linkhints-hint--hidden");
        if (typedLower.length > 0 && label === typedLower) {
          marker.classList.add("nr-linkhints-hint--matched");
        } else {
          marker.classList.remove("nr-linkhints-hint--matched");
        }
        // Dim unmatched characters
        const chars = marker.querySelectorAll(".nr-linkhints-char");
        chars.forEach((charEl, i) => {
          if (i < typedLower.length) {
            charEl.classList.remove("nr-linkhints-char--dimmed");
          } else {
            charEl.classList.add("nr-linkhints-char--dimmed");
          }
        });
        matchedIndices.push(parseInt(marker.dataset.elementIndex ?? "0", 10));
      } else {
        marker.classList.add("nr-linkhints-hint--hidden");
        marker.classList.remove("nr-linkhints-hint--matched");
      }
    }

    return matchedIndices;
  }

  /**
   * Get the element index for a fully matched hint label.
   * Returns undefined if no single match or no match.
   */
  getMatchedElementIndex(typedChars: string): number | undefined {
    const typedLower = typedChars.toLowerCase();
    let matchIndex: number | undefined;

    for (const marker of this.hintElements) {
      const label = (marker.dataset.label ?? "").toLowerCase();
      if (label === typedLower) {
        if (matchIndex !== undefined) {
          // Multiple matches - ambiguous
          return undefined;
        }
        matchIndex = parseInt(marker.dataset.elementIndex ?? "0", 10);
      }
    }

    return matchIndex;
  }

  /**
   * Check if any hints match the typed characters prefix.
   */
  hasMatchingHints(typedChars: string): boolean {
    const typedLower = typedChars.toLowerCase();
    return this.hintElements.some(
      (marker) => (marker.dataset.label ?? "").toLowerCase().startsWith(typedLower),
    );
  }

  /**
   * Clean up all overlay elements.
   */
  cleanup(doc: Document): void {
    // Remove hint elements
    this.hintElements = [];

    // Remove container
    this.container?.remove();
    this.container = null;

    // Remove styles
    const style = doc.getElementById(HINT_STYLE_ID);
    style?.remove();
    this.styleElement = null;
  }

  private positionMarker(marker: HTMLSpanElement, rect: DOMRect): void {
    marker.style.left = `${rect.left}px`;
    // Use fixed offset instead of marker.offsetHeight to avoid dependency
    // on the element being in the DOM at call time.
    marker.style.top = `${rect.top - HINT_MARKER_HEIGHT - 2}px`;
  }
}
