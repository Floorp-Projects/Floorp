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
  fingerprint?: string;
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
  targetFingerprint?: string;
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
  // getText options
  includeSelectorMap?: boolean;
  mode?: "full" | "scoped" | "visible";
  viewportMargin?: number;
  enableFingerprints?: boolean;
  // Click options
  button?: "left" | "right" | "middle";
  clickCount?: number;
  force?: boolean;
  // Accessibility tree options
  interestingOnly?: boolean;
  root?: string;
  // waitForNetworkIdle options
  maxInflight?: number;
  idleDuration?: number;
  ignorePatterns?: string[];
  state?: "attached" | "visible" | "hidden" | "detached";
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

/**
 * Mozilla Xray wrapper types
 * wrappedJSObject provides access to the underlying page object
 */
export interface XrayWrapped<T> {
  wrappedJSObject: T;
}

/**
 * Content window with wrappedJSObject access
 */
export type ContentWindow = Window &
  typeof globalThis &
  Partial<XrayWrapped<RawContentWindow>>;

/**
 * Raw content window (unwrapped) with native constructors
 */
export interface RawContentWindow extends Window {
  PointerEvent: typeof PointerEvent;
  MouseEvent: typeof MouseEvent;
  FocusEvent: typeof FocusEvent;
  Event: typeof Event;
  KeyboardEvent: typeof KeyboardEvent;
  InputEvent: typeof InputEvent;
  DragEvent: typeof DragEvent;
  DataTransfer: typeof DataTransfer;
  MutationObserver: typeof MutationObserver;
  HTMLInputElement: typeof HTMLInputElement;
  HTMLTextAreaElement: typeof HTMLTextAreaElement;
  HTMLSelectElement: typeof HTMLSelectElement;
}

/**
 * Element with optional wrappedJSObject
 */
export type XrayElement<T extends Element = Element> = T &
  Partial<XrayWrapped<T>>;

/**
 * HTMLElement with optional wrappedJSObject
 */
export type XrayHTMLElement = XrayElement<HTMLElement>;

/**
 * HTMLInputElement with optional wrappedJSObject
 */
export type XrayInputElement = XrayElement<HTMLInputElement>;

/**
 * HTMLTextAreaElement with optional wrappedJSObject
 */
export type XrayTextAreaElement = XrayElement<HTMLTextAreaElement>;

/**
 * HTMLSelectElement with optional wrappedJSObject
 */
export type XraySelectElement = XrayElement<HTMLSelectElement>;

/**
 * Union type for input-like elements
 */
export type XrayInputLikeElement = XrayInputElement | XrayTextAreaElement;

// =============================================================================
// getText options
// =============================================================================

/**
 * Options for getText() content extraction
 */
export interface GetTextOptions {
  /** Conversion mode: "full" (entire body), "scoped" (single selector), "visible" (viewport only) */
  mode?: "full" | "scoped" | "visible";
  /** CSS selector for "scoped" mode */
  selector?: string;
  /** Extra margin (px) beyond the viewport for "visible" mode (default: 500) */
  viewportMargin?: number;
  /** Embed fingerprints in Markdown output (default: true) */
  enableFingerprints?: boolean;
  /** Append selector map at end of output (default: false) */
  includeSelectorMap?: boolean;
}

// =============================================================================
// Click options
// =============================================================================

/**
 * Options for coordinate-based clickElement
 */
export interface ClickElementOptions {
  /** Mouse button (default: "left") */
  button?: "left" | "right" | "middle";
  /** Number of clicks (default: 1, use 2 for double-click) */
  clickCount?: number;
  /** Skip actionability checks (default: false) */
  force?: boolean;
  /** Timeout in ms for retries (default: 5000) */
  timeout?: number;
  /** Delay in ms for position stability check (default: 100, 0 to skip) */
  stabilityTimeout?: number;
}

// =============================================================================
// Accessibility Tree
// =============================================================================

/**
 * A node in the accessibility tree
 */
export interface AXNode {
  role: string;
  name: string;
  value?: string;
  description?: string;
  disabled?: boolean;
  expanded?: boolean;
  checked?: boolean | "mixed";
  selected?: boolean;
  level?: number;
  fingerprint?: string;
  children: AXNode[];
}

// =============================================================================
// Article extraction result
// =============================================================================

/**
 * Result from Readability-based article extraction
 */
export interface ArticleResult {
  title: string;
  byline: string;
  markdown: string;
  length: number;
}

// =============================================================================
// JavaScript Evaluation
// =============================================================================

/**
 * Result from evaluating JavaScript in the page context.
 * The `result` field is always JSON-serializable.
 */
export interface EvaluateResult {
  /** Whether evaluation completed without throwing */
  success: boolean;
  /** JSON-serializable result value (omitted on error) */
  result?: unknown;
  /** JS type of the returned value: "string" | "number" | "boolean" | "object" | "array" | "undefined" | "null" | "function" | "element" | etc. */
  resultType?: string;
  /** Error message (when success is false) */
  error?: string;
  /** Error constructor name, e.g. "SyntaxError", "TypeError" (when success is false) */
  errorType?: string;
}

// =============================================================================
// Actor message type map (for type-safe sendQuery)
// =============================================================================

/**
 * Maps actor message names to their request/response types.
 * Used by typed wrappers to ensure compile-time safety for
 * sendQuery calls.
 */
export interface WebScraperMessages {
  "WebScraper:GetHTML": {
    request: { selector?: string };
    response: string | null;
  };
  "WebScraper:GetText": {
    request: GetTextOptions;
    response: string | null;
  };
  "WebScraper:GetAccessibilityTree": {
    request: { interestingOnly?: boolean; root?: string };
    response: AXNode | null;
  };
  "WebScraper:GetArticle": {
    request: void;
    response: ArticleResult | null;
  };
  "WebScraper:ClickElement": {
    request: { selector: string } & ClickElementOptions;
    response: boolean;
  };
  "WebScraper:ResolveFingerprint": {
    request: { fingerprint: string };
    response: string | null;
  };
  "WebScraper:WaitForReady": {
    request: { timeout?: number };
    response: boolean;
  };
  "WebScraper:WaitForElement": {
    request: { selector: string; timeout?: number; state?: string };
    response: boolean;
  };
  "WebScraper:Evaluate": {
    request: { script: string };
    response: EvaluateResult;
  };
}
