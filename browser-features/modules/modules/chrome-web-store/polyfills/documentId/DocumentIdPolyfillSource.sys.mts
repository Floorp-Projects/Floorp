/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DocumentId Polyfill Source Code
 *
 * This module exports the JavaScript source code that gets injected into
 * converted Chrome extensions to provide documentId compatibility.
 *
 * The source code is a self-contained IIFE that installs the polyfill
 * when loaded.
 */

export const DOCUMENT_ID_POLYFILL_SOURCE = `
(function() {
  // DocumentId API Polyfill for Firefox
  // Generated from DocumentIdPolyfill.sys.mts

  const DOCUMENT_ID_PREFIX = "floorp-doc-";
  const DOCUMENT_ID_SEPARATOR = "-";

  class DocumentIdPolyfillImpl {
    constructor(options) {
      this.debugMode = options?.debug ?? false;
      this.instanceCounter = 0;
      this.documentIdCache = new Map();
    }

    generateDocumentId(tabId, frameId) {
      const instanceId = this.generateInstanceId();
      const documentId = \`\${DOCUMENT_ID_PREFIX}\${tabId}\${DOCUMENT_ID_SEPARATOR}\${frameId}\${DOCUMENT_ID_SEPARATOR}\${instanceId}\`;
      
      this.documentIdCache.set(documentId, { tabId, frameId, instanceId });
      this.log("Generated documentId:", documentId);
      return documentId;
    }

    parseDocumentId(documentId) {
      const cached = this.documentIdCache.get(documentId);
      if (cached) return cached;

      if (!documentId.startsWith(DOCUMENT_ID_PREFIX)) {
        this.log("Invalid documentId prefix:", documentId);
        return null;
      }

      const parts = documentId.slice(DOCUMENT_ID_PREFIX.length).split(DOCUMENT_ID_SEPARATOR);
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

      const parsed = { tabId, frameId, instanceId };
      this.documentIdCache.set(documentId, parsed);
      return parsed;
    }

    isValidDocumentId(documentId) {
      return this.parseDocumentId(documentId) !== null;
    }

    documentIdsToTargets(documentIds) {
      const targets = [];
      for (const docId of documentIds) {
        const parsed = this.parseDocumentId(docId);
        if (parsed) {
          targets.push({ tabId: parsed.tabId, frameId: parsed.frameId });
        }
      }
      return targets;
    }

    generateInstanceId() {
      this.instanceCounter++;
      const timestamp = Date.now().toString(36);
      const counter = this.instanceCounter.toString(36);
      return \`\${timestamp}\${counter}\`;
    }

    log(...args) {
      if (this.debugMode) {
        console.log("[Floorp DocumentId Polyfill]", ...args);
      }
    }
  }

  function wrapScriptingAPI(polyfill, originalScripting) {
    if (!originalScripting) return originalScripting;

    const originalExecuteScript = originalScripting.executeScript?.bind(originalScripting);
    if (!originalExecuteScript) return originalScripting;

    const wrappedExecuteScript = async (details) => {
      if (details.target?.documentIds && Array.isArray(details.target.documentIds)) {
        const targets = polyfill.documentIdsToTargets(details.target.documentIds);
        
        if (targets.length === 0) {
          throw new Error("No valid documentIds provided");
        }

        const results = [];
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

  function wrapTabsAPI(polyfill, originalTabs) {
    if (!originalTabs) return originalTabs;

    const originalSendMessage = originalTabs.sendMessage?.bind(originalTabs);
    if (!originalSendMessage) return originalTabs;

    const wrappedSendMessage = (tabId, message, options) => {
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

  function installDocumentIdPolyfill(options) {
    if (typeof chrome === "undefined") {
      console.warn("[Floorp DocumentId Polyfill] Not in extension context");
      return false;
    }

    const polyfill = new DocumentIdPolyfillImpl({ debug: options?.debug });

    // Store polyfill instance for access if needed
    if (!chrome.__floorpPolyfills) {
      chrome.__floorpPolyfills = {};
    }
    chrome.__floorpPolyfills.documentId = polyfill;

    if (chrome.scripting) {
      chrome.scripting = wrapScriptingAPI(polyfill, chrome.scripting);
    }

    if (chrome.tabs) {
      chrome.tabs = wrapTabsAPI(polyfill, chrome.tabs);
    }

    if (typeof browser !== "undefined") {
      if (!browser.__floorpPolyfills) {
        browser.__floorpPolyfills = {};
      }
      browser.__floorpPolyfills.documentId = polyfill;

      if (browser.scripting) {
        browser.scripting = wrapScriptingAPI(polyfill, browser.scripting);
      }
      if (browser.tabs) {
        browser.tabs = wrapTabsAPI(polyfill, browser.tabs);
      }
    }

    if (options?.debug) {
      console.log("[Floorp DocumentId Polyfill] Successfully installed");
    }

    return true;
  }

  // Auto-install
  if (typeof window !== "undefined" && !("__FLOORP_DOCUMENTID_MODULE__" in window)) {
    window.__FLOORP_DOCUMENTID_MODULE__ = true;
    installDocumentIdPolyfill({ debug: false });
  }
})();
`;
