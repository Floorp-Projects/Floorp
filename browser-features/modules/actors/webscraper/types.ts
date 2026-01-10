/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Type definitions for NRWebScraperChild
 */

export type HighlightScrollBehavior = ScrollBehavior | "none";

export interface NRWebScraperMessageData {
  selector?: string;
  value?: string;
  textContent?: string;
  timeout?: number;
  script?: string;
  rect?: { x?: number; y?: number; width?: number; height?: number };
  formData?: { [selector: string]: string };
  elementInfo?: string;
  attributeName?: string;
  checked?: boolean;
  optionValue?: string;
  targetSelector?: string;
  innerHTML?: string;
  eventType?: string;
  eventOptions?: { bubbles?: boolean; cancelable?: boolean };
  typingMode?: boolean;
  typingDelayMs?: number;
  filePath?: string;
  key?: string;
  cookieString?: string;
  cookieName?: string;
  cookieValue?: string;
}

export interface NormalizedHighlightOptions {
  action: string;
  duration: number;
  focus: boolean;
  scrollBehavior: HighlightScrollBehavior;
  padding: number;
  delay: number;
}

export type HighlightOptionsInput = Partial<NormalizedHighlightOptions> & {
  action?: string;
};

export type HighlightFocusOptions = {
  preventScroll?: boolean;
};

export interface InputElementOptions {
  typingMode?: boolean;
  typingDelayMs?: number;
  skipHighlight?: boolean;
}

export interface SelectOptionOptions {
  skipHighlight?: boolean;
}

export interface FillFormOptions {
  typingMode?: boolean;
  typingDelayMs?: number;
}

export interface DispatchEventOptions {
  bubbles?: boolean;
  cancelable?: boolean;
}

export interface ScreenshotRect {
  x?: number;
  y?: number;
  width?: number;
  height?: number;
}

/**
 * Context interface for child actor operations
 * Provides access to content window and document
 */
export interface WebScraperContext {
  contentWindow: (Window & typeof globalThis) | null;
  document: Document | null;
  sendQuery: (name: string, data?: unknown) => Promise<unknown>;
}
