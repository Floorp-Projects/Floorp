/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/** Actions that can be performed when a hint is selected */
export type LinkHintAction =
  | "openCurrentTab"
  | "openNewTab"
  | "openNewBackgroundTab"
  | "copyUrl"
  | "hover";

/** Data sent from chrome to content to activate link hints */
export interface LinkHintsActivateData {
  action: LinkHintAction;
}

/** Information about a detected clickable element */
export interface ClickableElementInfo {
  /** Reference to the DOM element (for hover and other content-side actions) */
  element: HTMLElement;
  /** The element's bounding rect in viewport coordinates */
  rect: DOMRect;
  /** The URL if the element is a link */
  href: string | null;
  /** Text content of the element */
  text: string | null;
  /** Tag name */
  tagName: string;
  /** Whether the element is likely a false positive */
  possibleFalsePositive: boolean;
}

/** Hint descriptor for a single hint marker */
export interface HintDescriptor {
  /** The hint label string (e.g., "AS", "DF") */
  label: string;
  /** Index into the clickable elements array */
  elementIndex: number;
  /** Position for the hint marker */
  rect: DOMRect;
}

/** Data sent from content to chrome when element is selected */
export interface LinkHintsElementSelectedData {
  href: string | null;
  text: string | null;
  tagName: string;
  action: LinkHintAction;
}

/** Data sent from content to chrome when hints are cancelled */
export interface LinkHintsCancelledData {
  reason: "escape" | "deactivate" | "noElements";
}

/** Data sent from content to chrome when hints are shown */
export interface LinkHintsShownData {
  count: number;
}

/** All valid link hint action strings */
export const VALID_LINK_HINT_ACTIONS: readonly string[] = [
  "openCurrentTab",
  "openNewTab",
  "openNewBackgroundTab",
  "copyUrl",
  "hover",
] as const;
