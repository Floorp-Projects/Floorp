/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * documentId Polyfill for Firefox
 *
 * Provides compatibility for Chrome's documentId property which is used in:
 * - chrome.scripting.executeScript() target.documentIds
 * - chrome.tabs.sendMessage() options
 * - Event handler callback arguments (e.g., webNavigation events)
 *
 * Since Firefox doesn't support documentId natively, this polyfill generates
 * synthetic documentIds based on tabId and frameId, and provides wrapper
 * functions to handle the translation.
 *
 * Chrome introduced documentId in Chrome 106+ to uniquely identify document
 * instances across navigations within the same frame.
 */

// =============================================================================
// Types
// =============================================================================

/**
 * Represents a parsed documentId
 */
export interface ParsedDocumentId {
  /** Tab ID */
  tabId: number;
  /** Frame ID (0 for main frame) */
  frameId: number;
  /** Unique identifier for the document instance */
  instanceId: string;
}

/**
 * Options for the DocumentIdPolyfill
 */
export interface DocumentIdPolyfillOptions {
  /** Enable debug logging */
  debug?: boolean;
}

/**
 * DocumentId API interface
 */
export interface DocumentIdAPI {
  /**
   * Generate a documentId from tab and frame information
   */
  generateDocumentId(tabId: number, frameId: number): string;

  /**
   * Parse a documentId into its components
   */
  parseDocumentId(documentId: string): ParsedDocumentId | null;

  /**
   * Check if a documentId is valid
   */
  isValidDocumentId(documentId: string): boolean;

  /**
   * Convert documentIds to tabId/frameId pairs for Firefox API calls
   */
  documentIdsToTargets(
    documentIds: string[],
  ): Array<{ tabId: number; frameId: number }>;
}

// =============================================================================
// Constants
// =============================================================================

/** Prefix for synthetic documentIds */
const DOCUMENT_ID_PREFIX = "floorp-doc-";

/** Separator between components in documentId */
const DOCUMENT_ID_SEPARATOR = "-";

/** Maximum cache size to prevent memory leaks */
const MAX_CACHE_SIZE = 1000;

// =============================================================================
// Implementation
// =============================================================================

/**
 * DocumentId Polyfill Implementation
 *
 * Generates and manages synthetic documentIds for Firefox compatibility.
 */
export class DocumentIdPolyfill implements DocumentIdAPI {
  private debugMode: boolean;
  private instanceCounter: number = 0;
  private documentIdCache: Map<string, ParsedDocumentId> = new Map();

  constructor(options?: DocumentIdPolyfillOptions) {
    this.debugMode = options?.debug ?? false;
  }

  /**
   * Generate a unique documentId from tab and frame information
   *
   * Format: floorp-doc-{tabId}-{frameId}-{instanceId}
   */
  generateDocumentId(tabId: number, frameId: number): string {
    const instanceId = this.generateInstanceId();
    const documentId =
      `${DOCUMENT_ID_PREFIX}${tabId}${DOCUMENT_ID_SEPARATOR}${frameId}${DOCUMENT_ID_SEPARATOR}${instanceId}`;

    // Prune cache if it exceeds max size
    if (this.documentIdCache.size >= MAX_CACHE_SIZE) {
      this.pruneCache();
    }

    this.documentIdCache.set(documentId, { tabId, frameId, instanceId });
    this.log("Generated documentId:", documentId);
    return documentId;
  }

  /**
   * Parse a documentId into its components
   */
  parseDocumentId(documentId: string): ParsedDocumentId | null {
    const cached = this.documentIdCache.get(documentId);
    if (cached) {
      return cached;
    }

    if (!documentId.startsWith(DOCUMENT_ID_PREFIX)) {
      this.log("Invalid documentId prefix:", documentId);
      return null;
    }

    const parts = documentId
      .slice(DOCUMENT_ID_PREFIX.length)
      .split(DOCUMENT_ID_SEPARATOR);
    if (parts.length < 3) {
      this.log("Invalid documentId format:", documentId);
      return null;
    }

    const tabId = parseInt(parts[0], 10);
    const frameId = parseInt(parts[1], 10);
    const instanceId = parts.slice(2).join(DOCUMENT_ID_SEPARATOR);

    if (isNaN(tabId) || isNaN(frameId)) {
      this.log("Invalid documentId numbers:", documentId);
      return null;
    }

    const parsed: ParsedDocumentId = { tabId, frameId, instanceId };
    this.documentIdCache.set(documentId, parsed);
    return parsed;
  }

  /**
   * Check if a documentId is valid
   */
  isValidDocumentId(documentId: string): boolean {
    return this.parseDocumentId(documentId) !== null;
  }

  /**
   * Convert documentIds to tabId/frameId pairs for Firefox API calls
   */
  documentIdsToTargets(
    documentIds: string[],
  ): Array<{ tabId: number; frameId: number }> {
    const targets: Array<{ tabId: number; frameId: number }> = [];

    for (const documentId of documentIds) {
      const parsed = this.parseDocumentId(documentId);
      if (parsed) {
        targets.push({ tabId: parsed.tabId, frameId: parsed.frameId });
      } else {
        this.log("Skipping invalid documentId:", documentId);
      }
    }

    return targets;
  }

  /**
   * Clear the documentId cache
   */
  clearCache(): void {
    this.documentIdCache.clear();
    this.log("Cache cleared");
  }

  private generateInstanceId(): string {
    this.instanceCounter++;
    const timestamp = Date.now().toString(36);
    const counter = this.instanceCounter.toString(36);
    return `${timestamp}${counter}`;
  }

  private log(...args: unknown[]): void {
    if (this.debugMode) {
      console.log("[Floorp DocumentId Polyfill]", ...args);
    }
  }

  /**
   * Prune the cache by removing the oldest entries
   * Removes approximately half of the cache to avoid frequent pruning
   */
  private pruneCache(): void {
    const entriesToRemove = Math.floor(this.documentIdCache.size / 2);
    const iterator = this.documentIdCache.keys();

    for (let i = 0; i < entriesToRemove; i++) {
      const result = iterator.next();
      if (result.done) break;
      this.documentIdCache.delete(result.value);
    }

    this.log(`Pruned ${entriesToRemove} entries from cache`);
  }
}

// =============================================================================
// API Wrappers
// =============================================================================

/**
 * Wrap chrome.scripting.executeScript to handle documentIds
 */
// deno-lint-ignore no-explicit-any
export function wrapScriptingAPI(polyfill: DocumentIdPolyfill, originalScripting: any): any {
  if (!originalScripting) {
    return originalScripting;
  }

  const originalExecuteScript = originalScripting.executeScript?.bind(originalScripting);
  if (!originalExecuteScript) {
    return originalScripting;
  }

  // deno-lint-ignore no-explicit-any
  const wrappedExecuteScript = async (details: any) => {
    if (details.target?.documentIds && Array.isArray(details.target.documentIds)) {
      const targets = polyfill.documentIdsToTargets(details.target.documentIds);

      if (targets.length === 0) {
        throw new Error("No valid documentIds provided");
      }

      // deno-lint-ignore no-explicit-any
      const results: any[] = [];
      for (const target of targets) {
        const modifiedDetails = {
          ...details,
          target: {
            ...details.target,
            tabId: target.tabId,
            frameIds: [target.frameId],
          },
        };
        delete modifiedDetails.target.documentIds;

        const result = await originalExecuteScript(modifiedDetails);
        results.push(...result);
      }
      return results;
    }

    return originalExecuteScript(details);
  };

  return {
    ...originalScripting,
    executeScript: wrappedExecuteScript,
  };
}

/**
 * Wrap chrome.tabs.sendMessage to handle documentId option
 */
// deno-lint-ignore no-explicit-any
export function wrapTabsAPI(polyfill: DocumentIdPolyfill, originalTabs: any): any {
  if (!originalTabs) {
    return originalTabs;
  }

  const originalSendMessage = originalTabs.sendMessage?.bind(originalTabs);
  if (!originalSendMessage) {
    return originalTabs;
  }

  // deno-lint-ignore no-explicit-any
  const wrappedSendMessage = (tabId: number, message: any, options?: any) => {
    if (options?.documentId) {
      const parsed = polyfill.parseDocumentId(options.documentId);

      if (!parsed) {
        return Promise.reject(new Error("Invalid documentId"));
      }

      const modifiedOptions = { ...options, frameId: parsed.frameId };
      delete modifiedOptions.documentId;

      return originalSendMessage(tabId, message, modifiedOptions);
    }

    return originalSendMessage(tabId, message, options);
  };

  return {
    ...originalTabs,
    sendMessage: wrappedSendMessage,
  };
}

// =============================================================================
// Installation
// =============================================================================

/**
 * Install the documentId polyfill into chrome/browser objects
 */
export function installDocumentIdPolyfill(
  options?: DocumentIdPolyfillOptions & { force?: boolean },
): boolean {
  if (typeof chrome === "undefined") {
    console.warn("[Floorp DocumentId Polyfill] Not in extension context");
    return false;
  }

  const polyfill = new DocumentIdPolyfill({ debug: options?.debug });

  if (chrome.scripting) {
    // deno-lint-ignore no-explicit-any
    (chrome as any).scripting = wrapScriptingAPI(polyfill, chrome.scripting);
  }

  if (chrome.tabs) {
    // deno-lint-ignore no-explicit-any
    (chrome as any).tabs = wrapTabsAPI(polyfill, chrome.tabs);
  }

  if (typeof browser !== "undefined") {
    if (browser.scripting) {
      // deno-lint-ignore no-explicit-any
      (browser as any).scripting = wrapScriptingAPI(polyfill, browser.scripting);
    }
    if (browser.tabs) {
      // deno-lint-ignore no-explicit-any
      (browser as any).tabs = wrapTabsAPI(polyfill, browser.tabs);
    }
  }

  if (options?.debug) {
    console.log("[Floorp DocumentId Polyfill] Successfully installed");
  }

  return true;
}

// =============================================================================
// Global declarations
// =============================================================================

/* eslint-disable no-var */
declare global {
  var chrome: {
    // deno-lint-ignore no-explicit-any
    scripting?: any;
    // deno-lint-ignore no-explicit-any
    tabs?: any;
    // deno-lint-ignore no-explicit-any
    [key: string]: any;
  };

  var browser: {
    // deno-lint-ignore no-explicit-any
    scripting?: any;
    // deno-lint-ignore no-explicit-any
    tabs?: any;
    // deno-lint-ignore no-explicit-any
    [key: string]: any;
  };
}
/* eslint-enable no-var */

