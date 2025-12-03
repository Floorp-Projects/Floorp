/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRChromeWebStoreChild - Content process actor for Chrome Web Store integration
 *
 * This actor runs in the content process and provides functionality to:
 * - Inject "Add to Floorp" button on Chrome Web Store extension pages
 * - Extract extension metadata from the page
 * - Communicate with the parent process to initiate installation
 *
 * @module NRChromeWebStoreChild
 */

import type {
  ExtensionMetadata,
  MutableExtensionMetadata,
} from "../modules/chrome-web-store/types.ts";
import {
  isChromeWebStoreUrl,
  extractExtensionId,
  CWS_I18N_KEYS,
} from "../modules/chrome-web-store/Constants.sys.mts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// =============================================================================
// Constants
// =============================================================================

/** Main loop interval in milliseconds (following Fire-CWS approach) */
const MAIN_LOOP_INTERVAL = 100;

/** Button state reset delay in milliseconds */
const BUTTON_RESET_DELAY = 3000;

/** Error notice auto-hide delay in milliseconds */
const ERROR_NOTICE_TIMEOUT = 10000;

/** Button states */
type ButtonState = "default" | "installing" | "success" | "error";

/**
 * Cache for translations to avoid repeated lookups
 * Content scripts can't directly access chrome-only I18nUtils,
 * so we request translations from the parent process
 */
let translationCache: Record<string, string> = {};
let translationCacheInitialized = false;

/**
 * Default translations (English) used when i18n is not yet initialized
 */
const DEFAULT_TRANSLATIONS: Record<string, string> = {
  [CWS_I18N_KEYS.button.addToFloorp]: "Add to Floorp",
  [CWS_I18N_KEYS.button.installing]: "Installing...",
  [CWS_I18N_KEYS.button.success]: "Added!",
  [CWS_I18N_KEYS.button.error]: "Error occurred",
  [CWS_I18N_KEYS.error.title]: "Installation Error",
  [CWS_I18N_KEYS.error.compatibilityNote]:
    "This extension may not be compatible with Firefox/Floorp.",
  [CWS_I18N_KEYS.error.installFailed]: "Installation failed",
};

// =============================================================================
// DOM Structure-based Selectors (Language-independent)
// =============================================================================
// Chrome Web Store uses a consistent DOM structure:
// - main > div > section (first section contains the extension header)
// - The header section contains: h1 (extension name), img (logo), button (Add to Chrome)
// - Other sections contain: 概要/Overview, 詳細/Details, プライバシー/Privacy, etc.
//
// This approach uses structural selectors instead of text-based patterns,
// making it work across all languages and UI changes.

/**
 * Primary selector for the "Add to Chrome" button.
 * Uses DOM structure: main element contains sections, first section has h1 and the action button.
 * The button is a sibling of the div containing h1.
 */
const ADD_BUTTON_STRUCTURE_SELECTOR =
  "main section:has(h1) button:first-of-type";

/**
 * Fallback selector using the first section approach
 */
const ADD_BUTTON_FALLBACK_SELECTOR =
  "main > div > section:first-of-type button:first-of-type";

/**
 * Selector for extension logo image (fallback for extractExtensionMetadata)
 */
const EXTENSION_LOGO_SELECTOR = "main section:has(h1) img";

// =============================================================================
// Main Class
// =============================================================================

export class NRChromeWebStoreChild extends JSWindowActorChild {
  private buttonInjected = false;
  private styleInjected = false;
  private currentExtensionId: string | null = null;
  private installInProgress = false;
  private lastUrl: string | null = null;
  private mainLoopInterval: number | null = null;
  private hiddenAddToChromeButton = false;
  private experimentEnabled: boolean | null = null;

  /**
   * Get a translation for a key, using cache or default values
   * @param key - Translation key
   * @param vars - Optional variables for interpolation
   * @returns Translated string
   */
  private t(key: string, vars?: Record<string, string | number>): string {
    // Check cache first, then fall back to defaults
    let result = translationCache[key] || DEFAULT_TRANSLATIONS[key] || key;

    if (vars) {
      for (const [k, v] of Object.entries(vars)) {
        result = result.replace(new RegExp(`\\{\\{${k}\\}\\}`, "g"), String(v));
      }
    }
    return result;
  }

  /**
   * Initialize translation cache by requesting from parent process
   * Retries if translations are not available yet (i18n not initialized)
   */
  private async initTranslations(retryCount = 0): Promise<void> {
    if (translationCacheInitialized) return;

    const MAX_RETRIES = 5;
    const RETRY_DELAY = 200;

    try {
      // Request all CWS translations from parent
      const translations = (await this.sendQuery("CWS:GetTranslations", {
        keys: [
          CWS_I18N_KEYS.button.addToFloorp,
          CWS_I18N_KEYS.button.installing,
          CWS_I18N_KEYS.button.success,
          CWS_I18N_KEYS.button.error,
          CWS_I18N_KEYS.error.title,
          CWS_I18N_KEYS.error.compatibilityNote,
          CWS_I18N_KEYS.error.installFailed,
        ],
      })) as Record<string, string> | null;

      if (translations) {
        // Check if translations are actually loaded (not just keys returned)
        const firstKey = CWS_I18N_KEYS.button.addToFloorp;
        if (translations[firstKey] && translations[firstKey] !== firstKey) {
          translationCache = { ...translationCache, ...translations };
          translationCacheInitialized = true;
        } else if (retryCount < MAX_RETRIES) {
          // i18n not initialized yet, retry after delay
          await new Promise((resolve) => setTimeout(resolve, RETRY_DELAY));
          return this.initTranslations(retryCount + 1);
        }
      }
    } catch {
      // Translation request failed, retry if possible
      if (retryCount < MAX_RETRIES) {
        await new Promise((resolve) => setTimeout(resolve, RETRY_DELAY));
        return this.initTranslations(retryCount + 1);
      }
      console.debug("[NRChromeWebStore] Failed to load translations");
    }
  }

  /**
   * Check if the Chrome Web Store experiment is enabled
   * Results are cached for the lifetime of this actor
   */
  private async checkExperimentEnabled(): Promise<boolean> {
    if (this.experimentEnabled !== null) {
      return this.experimentEnabled;
    }

    try {
      const enabled = (await this.sendQuery(
        "CWS:CheckExperiment",
        {},
      )) as boolean;
      this.experimentEnabled = enabled;
      console.debug("[NRChromeWebStore] Experiment enabled:", enabled);
      return enabled;
    } catch (e) {
      console.warn("[NRChromeWebStore] Failed to check experiment:", e);
      this.experimentEnabled = false;
      return false;
    }
  }

  /**
   * Called when the actor is created
   */
  actorCreated(): void {
    console.debug(
      "[NRChromeWebStore] Child actor created for:",
      this.contentWindow?.location?.href,
    );
    // Initialize translations asynchronously
    this.initTranslations().catch(() => {});
  }

  /**
   * Handle DOM events
   */
  handleEvent(event: Event): void {
    switch (event.type) {
      case "DOMContentLoaded":
        this.onDOMContentLoaded();
        break;
      case "popstate":
        this.onUrlChange();
        break;
    }
  }

  /**
   * Handle DOMContentLoaded event
   */
  private onDOMContentLoaded(): void {
    const url = this.document?.location?.href;
    console.debug("[NRChromeWebStore] DOMContentLoaded:", url);

    this.lastUrl = url ?? null;

    if (!url || !isChromeWebStoreUrl(url)) {
      return;
    }

    // Check if experiment is enabled before proceeding
    this.checkExperimentEnabled().then((enabled) => {
      if (!enabled) {
        console.debug(
          "[NRChromeWebStore] Feature disabled by experiment, skipping initialization",
        );
        return;
      }

      // Inject styles
      this.injectStyles();

      // Start main loop for button injection and page modifications
      // Following Fire-CWS approach: simple interval-based checking
      this.startMainLoop();
    });
  }

  /**
   * Handle URL changes for SPA navigation
   */
  private onUrlChange(): void {
    const url = this.document?.location?.href;

    if (url === this.lastUrl) {
      return;
    }

    console.debug("[NRChromeWebStore] URL changed:", this.lastUrl, "->", url);
    this.lastUrl = url ?? null;

    // Reset state for new page
    this.buttonInjected = false;
    this.currentExtensionId = null;
    this.installInProgress = false;
    this.hiddenAddToChromeButton = false;

    // Remove existing button when navigating away
    this.removeExistingButton();
  }

  /**
   * Start the main loop for monitoring and modifying the page
   * Following Fire-CWS's simple interval-based approach
   */
  private startMainLoop(): void {
    if (this.mainLoopInterval) {
      clearTimeout(this.mainLoopInterval);
    }

    const mainLoop = () => {
      // Check for URL changes (SPA navigation)
      const currentUrl = this.document?.location?.href;
      if (currentUrl && currentUrl !== this.lastUrl) {
        this.onUrlChange();
      }

      // Hide "Add to Chrome" button after our button is injected
      this.hideAddToChromeButton();

      // Inject button on extension pages
      if (currentUrl && this.isChromeWebStoreExtensionPage(currentUrl)) {
        this.attemptButtonInjection();
      }

      // Continue the loop
      this.mainLoopInterval = setTimeout(
        mainLoop,
        MAIN_LOOP_INTERVAL,
      ) as unknown as number;
    };

    // Start the loop
    this.mainLoopInterval = setTimeout(
      mainLoop,
      MAIN_LOOP_INTERVAL,
    ) as unknown as number;
  }

  /**
   * Hide the original "Add to Chrome" button
   * This replaces the Chrome button with the Floorp button
   */
  private hideAddToChromeButton(): void {
    if (this.hiddenAddToChromeButton) return;

    const doc = this.document;
    if (!doc) return;

    // Only hide after we've injected our button
    if (!this.buttonInjected) return;

    // Find and hide the Add to Chrome button
    const addToChromeButton = this.findOriginalAddToChromeButton();
    if (addToChromeButton) {
      (addToChromeButton as HTMLElement).style.setProperty(
        "display",
        "none",
        "important",
      );
      this.hiddenAddToChromeButton = true;
      console.debug("[NRChromeWebStore] Hidden Add to Chrome button");
    }
  }

  /**
   * Find the original "Add to Chrome" button (not our Floorp button)
   */
  private findOriginalAddToChromeButton(): Element | null {
    const doc = this.document;
    if (!doc) return null;

    // Find the visible h1's section and get the button
    const allH1s = doc.querySelectorAll("h1");
    for (const h1 of allH1s) {
      if (!this.isElementVisible(h1)) continue;

      const section = h1.closest("section");
      if (!section) continue;

      // Find button that is NOT our Floorp button
      const buttons = section.querySelectorAll("button");
      for (const button of buttons) {
        if (
          button.id !== "floorp-add-extension-btn" &&
          this.isElementVisible(button)
        ) {
          return button;
        }
      }
    }

    return null;
  }

  /**
   * Remove the existing Floorp button
   */
  private removeExistingButton(): void {
    const doc = this.document;
    if (!doc) return;

    const existingButton = doc.getElementById("floorp-add-extension-btn");
    if (existingButton) {
      existingButton.remove();
    }

    const existingError = doc.getElementById("floorp-cws-error");
    if (existingError) {
      existingError.remove();
    }
  }

  /**
   * Check if URL is a Chrome Web Store extension detail page
   * @param url - URL to check
   */
  private isChromeWebStoreExtensionPage(url: string): boolean {
    return extractExtensionId(url) !== null;
  }

  /**
   * Inject CSS styles for the Floorp button
   */
  private injectStyles(): void {
    if (this.styleInjected) return;

    const doc = this.document;
    if (!doc) return;

    const style = doc.createElement("style");
    style.id = "floorp-cws-styles";
    style.textContent = `
      .floorp-add-button {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        gap: 8px;
        padding: 0 24px;
        height: 36px;
        margin: 8px;
        background: #0b57d0;
        color: white;
        border: none;
        border-radius: 20px;
        font-size: 14px;
        font-weight: 500;
        cursor: pointer;
        transition: all 0.2s ease;
        box-shadow: none;
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      }

      .floorp-add-button:hover {
        background: #0842a0;
        box-shadow: 0 1px 3px rgba(0, 0, 0, 0.3);
      }

      .floorp-add-button:active {
        background: #063281;
      }

      .floorp-add-button:disabled {
        background: #9ca3af;
        cursor: not-allowed;
        transform: none;
        box-shadow: none;
      }

      .floorp-add-button--installing {
        background: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
        box-shadow: 0 4px 15px rgba(245, 158, 11, 0.4);
      }

      .floorp-add-button--success {
        background: linear-gradient(135deg, #10b981 0%, #059669 100%);
        box-shadow: 0 4px 15px rgba(16, 185, 129, 0.4);
      }

      .floorp-add-button--error {
        background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
        box-shadow: 0 4px 15px rgba(239, 68, 68, 0.4);
      }

      .floorp-add-button__icon {
        width: 20px;
        height: 20px;
      }

      .floorp-add-button__spinner {
        width: 16px;
        height: 16px;
        border: 2px solid rgba(255, 255, 255, 0.3);
        border-top-color: white;
        border-radius: 50%;
        animation: floorp-spin 0.8s linear infinite;
      }

      @keyframes floorp-spin {
        to { transform: rotate(360deg); }
      }

      .floorp-cws-notice {
        display: flex;
        align-items: flex-start;
        gap: 12px;
        margin: 16px;
        padding: 16px;
        background: #fef3c7;
        border: 1px solid #fcd34d;
        border-radius: 12px;
        font-size: 13px;
        line-height: 1.5;
        color: #92400e;
      }

      .floorp-cws-notice--error {
        background: #fee2e2;
        border-color: #fca5a5;
        color: #991b1b;
      }

      .floorp-cws-notice__icon {
        flex-shrink: 0;
        width: 20px;
        height: 20px;
      }

      /* Hide Chrome promotional elements */
      [aria-labelledby="promo-header"] {
        display: none !important;
      }

      /* Hide Chrome download prompt container (main > div > section > div.gSrP5d) */
      main > div > section > div.gSrP5d {
        display: none !important;
      }
    `;

    (doc.head ?? doc.documentElement)?.appendChild(style);
    this.styleInjected = true;
  }

  /**
   * Check if the page is ready for button injection.
   * For SPA navigation, the URL changes before the DOM is fully updated.
   * We need to verify that the displayed content matches the current URL.
   *
   * @returns true if the page is ready, false otherwise
   */
  private isPageReadyForInjection(): boolean {
    const doc = this.document;
    if (!doc) return false;

    const locationHref = doc.location?.href;
    if (!locationHref) return false;

    const extensionId = extractExtensionId(locationHref);
    if (!extensionId) return false;

    // Find the visible h1 (extension name)
    const allH1s = doc.querySelectorAll("h1");
    let visibleH1: HTMLHeadingElement | null = null;
    for (const h1 of allH1s) {
      if (this.isElementVisible(h1)) {
        visibleH1 = h1 as HTMLHeadingElement;
        break;
      }
    }

    if (!visibleH1) {
      console.debug("[NRChromeWebStore] No visible h1 found, page not ready");
      return false;
    }

    // Check if the h1 is in a section
    const section = visibleH1.closest("section");
    if (!section) {
      console.debug("[NRChromeWebStore] h1 not in section, page not ready");
      return false;
    }

    // Verify the "Add to Chrome" button is visible and ready
    const button = section.querySelector("button");
    if (!button || !this.isElementVisible(button)) {
      console.debug(
        "[NRChromeWebStore] No visible button in section, page not ready",
      );
      return false;
    }

    // For SPA navigation: check if there are any links on the page containing the extension ID
    // This helps verify the page content matches the URL
    // Check the entire page for the extension ID (in links, report URLs, etc.)
    const allLinks = doc.querySelectorAll('a[href*="' + extensionId + '"]');
    if (allLinks.length === 0) {
      // Fallback: check if the main content area contains any reference to the extension
      // The "詳細" section typically has a "懸念事項を報告" link with the extension ID
      const mainContent = doc.querySelector("main");
      if (mainContent) {
        const mainHtml = String(mainContent.innerHTML);
        if (!mainHtml.includes(extensionId)) {
          console.debug(
            "[NRChromeWebStore] Page does not contain extension ID, page not ready",
            extensionId,
          );
          return false;
        }
      }
    }

    return true;
  }

  /**
   * Attempt to inject the Floorp button
   * Following Fire-CWS approach: simple button detection and injection
   * @returns true if injection was successful, false otherwise
   */
  private attemptButtonInjection(): boolean {
    if (this.buttonInjected) return true;

    const doc = this.document;
    if (!doc) return false;

    // For SPA navigation: verify the page is ready before injecting
    if (!this.isPageReadyForInjection()) {
      return false;
    }

    // Find the "Add to Chrome" button using multiple strategies
    const addToChromeButton = this.findAddToChromeButton();

    if (addToChromeButton) {
      return this.injectButtonNextTo(addToChromeButton);
    }

    // Fallback: try to find any suitable container
    const fallbackContainer = this.findFallbackContainer();
    if (fallbackContainer) {
      return this.injectButtonInContainer(fallbackContainer);
    }

    return false;
  }

  /**
   * Find the "Add to Chrome" button using DOM structure-based selectors.
   *
   * Chrome Web Store has a consistent DOM structure:
   * - main > div > section (first section = header with extension info)
   * - Header section contains: img (logo), h1 (name), and button (Add to Chrome)
   * - The "Add to Chrome" button is the first button in the header section
   *
   * This approach is language-independent and doesn't rely on button text.
   *
   * NOTE: For SPA navigation, Chrome Web Store may have multiple h1 elements
   * from different "pages". We need to find the VISIBLE h1 element.
   */
  private findAddToChromeButton(): Element | null {
    const doc = this.document;
    if (!doc) return null;

    // Strategy 1: Find the VISIBLE h1 element and its nearby button
    // This is the most reliable approach for SPA navigation
    const allH1s = doc.querySelectorAll("h1");
    for (const h1 of allH1s) {
      if (!this.isElementVisible(h1)) continue;

      // Check if this h1 is inside a section
      const section = h1.closest("section");
      if (!section) continue;

      // Look for the "Add to Chrome" button near this h1
      // DOM structure: section > div > div (contains h1) + div (contains button)
      const h1Container = h1.closest("div");
      const siblingDiv = h1Container?.nextElementSibling;
      const button = siblingDiv?.querySelector("button");

      if (
        button &&
        button.id !== "floorp-add-extension-btn" &&
        this.isElementVisible(button)
      ) {
        console.debug(
          "[NRChromeWebStore] Found button via visible h1 sibling strategy",
        );
        return button;
      }

      // Alternative: Find first button within the section
      const sectionButton = section.querySelector("button");
      if (
        sectionButton &&
        sectionButton.id !== "floorp-add-extension-btn" &&
        this.isElementVisible(sectionButton)
      ) {
        console.debug("[NRChromeWebStore] Found button via visible h1 section");
        return sectionButton;
      }
    }

    // Strategy 2: Use direct CSS selector (may not work with SPA)
    const buttonViaSelector = doc.querySelector(ADD_BUTTON_STRUCTURE_SELECTOR);
    if (
      buttonViaSelector &&
      buttonViaSelector.id !== "floorp-add-extension-btn" &&
      this.isElementVisible(buttonViaSelector)
    ) {
      console.debug("[NRChromeWebStore] Found button via structure selector");
      return buttonViaSelector;
    }

    // Strategy 3: Fallback to first section's first button
    const fallbackButton = doc.querySelector(ADD_BUTTON_FALLBACK_SELECTOR);
    if (
      fallbackButton &&
      fallbackButton.id !== "floorp-add-extension-btn" &&
      this.isElementVisible(fallbackButton)
    ) {
      console.debug("[NRChromeWebStore] Found button via fallback selector");
      return fallbackButton;
    }

    console.debug("[NRChromeWebStore] Could not find Add to Chrome button");
    return null;
  }

  /**
   * Find a fallback container for button injection.
   * Uses the header section structure if the Add to Chrome button is not found.
   *
   * NOTE: For SPA navigation, we need to find the VISIBLE h1 element's section.
   */
  private findFallbackContainer(): Element | null {
    const doc = this.document;
    if (!doc) return null;

    // Strategy 1: Find the VISIBLE h1 element's container
    const allH1s = doc.querySelectorAll("h1");
    for (const h1 of allH1s) {
      if (!this.isElementVisible(h1)) continue;

      const section = h1.closest("section");
      if (!section) continue;

      // The button container is typically a sibling of the h1 container
      const h1Container = h1.closest("div");
      const siblingDiv = h1Container?.nextElementSibling;
      if (siblingDiv && this.isElementVisible(siblingDiv)) {
        return siblingDiv;
      }
      // If no sibling, use the parent of h1 container
      if (h1Container?.parentElement) {
        return h1Container.parentElement;
      }
    }

    // Strategy 2: First visible section in main
    const sections = doc.querySelectorAll("main section");
    for (const section of sections) {
      if (!this.isElementVisible(section)) continue;

      // Find a div that contains or would contain action buttons
      const divs = section.querySelectorAll("div");
      for (const div of divs) {
        if (
          div.querySelector("button") ||
          div.querySelector('a[role="button"]')
        ) {
          return div;
        }
      }
    }

    return null;
  }

  /**
   * Check if an element is visible
   */
  private isElementVisible(element: Element): boolean {
    const rect = element.getBoundingClientRect();
    if (rect.width === 0 || rect.height === 0) return false;

    const style = this.contentWindow?.getComputedStyle(element);
    if (!style) return true;

    return (
      style.getPropertyValue("display") !== "none" &&
      style.getPropertyValue("visibility") !== "hidden" &&
      style.getPropertyValue("opacity") !== "0"
    );
  }

  /**
   * Inject the Floorp button next to the Add to Chrome button
   */
  private injectButtonNextTo(targetButton: Element): boolean {
    const doc = this.document;
    if (!doc || this.buttonInjected) return false;

    const locationHref = doc.location?.href;
    if (!locationHref) {
      console.error("[NRChromeWebStore] Could not get document location");
      return false;
    }

    const extensionId = extractExtensionId(locationHref);
    if (!extensionId) {
      console.error("[NRChromeWebStore] Could not extract extension ID");
      return false;
    }

    this.currentExtensionId = extensionId;

    // Create the Floorp button
    const button = this.createFloorpButton();

    // Insert after the Add to Chrome button
    targetButton.insertAdjacentElement("afterend", button);
    this.buttonInjected = true;

    console.log(
      "[NRChromeWebStore] Successfully injected Floorp button next to Add to Chrome for:",
      extensionId,
    );
    return true;
  }

  /**
   * Inject the Floorp button in a container
   */
  private injectButtonInContainer(container: Element): boolean {
    const doc = this.document;
    if (!doc || this.buttonInjected) return false;

    const locationHref = doc.location?.href;
    if (!locationHref) {
      console.error("[NRChromeWebStore] Could not get document location");
      return false;
    }

    const extensionId = extractExtensionId(locationHref);
    if (!extensionId) {
      console.error("[NRChromeWebStore] Could not extract extension ID");
      return false;
    }

    this.currentExtensionId = extensionId;

    // Create the Floorp button
    const button = this.createFloorpButton();

    // Append to container
    container.appendChild(button);
    this.buttonInjected = true;

    console.log(
      "[NRChromeWebStore] Successfully injected Floorp button in container for:",
      extensionId,
    );
    return true;
  }

  /**
   * Create the Floorp add extension button
   */
  private createFloorpButton(): HTMLButtonElement {
    const doc = this.document!;
    const button = doc.createElement("button");
    button.id = "floorp-add-extension-btn";
    button.className = "floorp-add-button";
    const buttonText = this.t(CWS_I18N_KEYS.button.addToFloorp);
    button.innerHTML = `
      <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
        <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z"/>
      </svg>
      <span>${this.escapeHtml(buttonText)}</span>
    `;

    button.addEventListener("click", () => this.handleInstallClick());
    return button;
  }

  /**
   * Handle click on the "Add to Floorp" button
   */
  private async handleInstallClick(): Promise<void> {
    if (this.installInProgress || !this.currentExtensionId) return;

    this.installInProgress = true;
    const button =
      this.document?.getElementById("floorp-add-extension-btn") ?? null;

    try {
      // Update button state to installing
      this.updateButtonState(button, "installing");

      // Extract extension metadata
      const metadata = this.extractExtensionMetadata();
      console.log("[NRChromeWebStore] Installing extension:", metadata);

      // Send install request to parent
      const result = (await this.sendQuery("CWS:InstallExtension", {
        extensionId: this.currentExtensionId,
        metadata,
      })) as { success: boolean; error?: string };

      if (result.success) {
        this.updateButtonState(button, "success");
        console.log("[NRChromeWebStore] Extension installed successfully");
      } else {
        this.updateButtonState(button, "error");
        this.showError(
          result.error || this.t(CWS_I18N_KEYS.error.installFailed),
        );
        console.error("[NRChromeWebStore] Installation failed:", result.error);
      }
    } catch (error) {
      this.updateButtonState(button, "error");
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      this.showError(errorMessage);
      console.error("[NRChromeWebStore] Installation error:", error);
    } finally {
      // Reset button state after delay
      setTimeout(() => {
        this.updateButtonState(button, "default");
        this.installInProgress = false;
      }, BUTTON_RESET_DELAY);
    }
  }

  /**
   * Update the button visual state
   */
  private updateButtonState(button: Element | null, state: ButtonState): void {
    if (!button) return;

    // Remove all state classes
    button.classList.remove(
      "floorp-add-button--installing",
      "floorp-add-button--success",
      "floorp-add-button--error",
    );

    // Update content and class based on state
    switch (state) {
      case "installing": {
        const installingText = this.t(CWS_I18N_KEYS.button.installing);
        button.classList.add("floorp-add-button--installing");
        button.innerHTML = `
          <div class="floorp-add-button__spinner"></div>
          <span>${this.escapeHtml(installingText)}</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      case "success": {
        const successText = this.t(CWS_I18N_KEYS.button.success);
        button.classList.add("floorp-add-button--success");
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/>
          </svg>
          <span>${this.escapeHtml(successText)}</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      case "error": {
        const errorText = this.t(CWS_I18N_KEYS.button.error);
        button.classList.add("floorp-add-button--error");
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C6.47 2 2 6.47 2 12s4.47 10 10 10 10-4.47 10-10S17.53 2 12 2zm5 13.59L15.59 17 12 13.41 8.41 17 7 15.59 10.59 12 7 8.41 8.41 7 12 10.59 15.59 7 17 8.41 13.41 12 17 15.59z"/>
          </svg>
          <span>${this.escapeHtml(errorText)}</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      default: {
        const defaultText = this.t(CWS_I18N_KEYS.button.addToFloorp);
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z"/>
          </svg>
          <span>${this.escapeHtml(defaultText)}</span>
        `;
        (button as HTMLButtonElement).disabled = false;
      }
    }
  }

  /**
   * Extract extension metadata from the current page using DOM structure.
   *
   * Chrome Web Store page structure:
   * - main h1: Extension name
   * - main section:has(h1) img: Extension icon
   * - Section with h2 "概要"/"Overview": Description paragraphs
   * - Section with h2 "詳細"/"Details": Version, author, size, etc. in list items
   *
   * NOTE: For SPA navigation, we need to find the VISIBLE h1 element.
   */
  private extractExtensionMetadata(): ExtensionMetadata {
    const doc = this.document;
    const metadata: MutableExtensionMetadata = {
      id: this.currentExtensionId || "",
      name: "",
    };

    if (!doc) return metadata;

    const main = doc.querySelector("main");
    if (!main) return metadata;

    // Extract extension name from VISIBLE h1
    // Chrome Web Store SPA keeps old page elements, so we need to find the visible one
    const allH1s = doc.querySelectorAll("h1");
    let visibleH1: HTMLHeadingElement | null = null;
    for (const h1 of allH1s) {
      if (this.isElementVisible(h1)) {
        visibleH1 = h1 as HTMLHeadingElement;
        break;
      }
    }

    if (visibleH1?.textContent) {
      metadata.name = visibleH1.textContent.trim();
    }

    // Extract icon URL from img in the same section as visible h1
    if (visibleH1) {
      const section = visibleH1.closest("section");
      const logoImg = section?.querySelector("img") as HTMLImageElement | null;
      if (logoImg?.src) {
        metadata.iconUrl = logoImg.src;
      }
    }

    // Fallback: try the old selector
    if (!metadata.iconUrl) {
      const logoImg = doc.querySelector(
        EXTENSION_LOGO_SELECTOR,
      ) as HTMLImageElement | null;
      if (logoImg?.src) {
        metadata.iconUrl = logoImg.src;
      }
    }

    // Find sections by their h2 headings (only visible sections)
    const sections = main.querySelectorAll("section");

    for (const section of sections) {
      if (!this.isElementVisible(section)) continue;

      const h2 = section.querySelector("h2");
      const h2Text = h2?.textContent?.toLowerCase() || "";

      // Extract description from Overview/概要 section
      if (
        h2Text.includes("概要") ||
        h2Text.includes("overview") ||
        h2Text.includes("summary")
      ) {
        const paragraph = section.querySelector("p");
        if (paragraph?.textContent) {
          metadata.description = paragraph.textContent.trim().slice(0, 500);
        }
      }

      // Extract version and author from Details/詳細 section
      if (
        h2Text.includes("詳細") ||
        h2Text.includes("detail") ||
        h2Text.includes("information")
      ) {
        const listItems = section.querySelectorAll("li");
        for (const li of listItems) {
          const text = li.textContent || "";
          const textLower = text.toLowerCase();

          // Version: list item containing version-related keywords
          if (
            textLower.includes("バージョン") ||
            textLower.includes("version")
          ) {
            // The version value is in the last child element
            const valueEl = li.querySelector("div:last-child, span:last-child");
            if (valueEl?.textContent) {
              const versionMatch = valueEl.textContent.match(/[\d.]+/);
              if (versionMatch) {
                metadata.version = versionMatch[0];
              }
            }
          }

          // Author: list item containing publisher/author keywords
          if (
            textLower.includes("提供元") ||
            textLower.includes("offered") ||
            textLower.includes("publisher") ||
            textLower.includes("author")
          ) {
            const valueEl = li.querySelector("div:last-child, span:last-child");
            if (valueEl?.textContent) {
              metadata.author = valueEl.textContent.trim();
            }
          }
        }
      }
    }

    return metadata;
  }

  /**
   * Show an error notice to the user
   */
  private showError(message: string): void {
    const doc = this.document;
    if (!doc) return;

    // Remove existing error notice
    const existing = doc.getElementById("floorp-cws-error");
    if (existing) {
      existing.remove();
    }

    const errorTitle = this.t(CWS_I18N_KEYS.error.title);
    const compatibilityNote = this.t(CWS_I18N_KEYS.error.compatibilityNote);

    const notice = doc.createElement("div");
    notice.id = "floorp-cws-error";
    notice.className = "floorp-cws-notice floorp-cws-notice--error";
    notice.innerHTML = `
      <svg class="floorp-cws-notice__icon" viewBox="0 0 24 24" fill="currentColor">
        <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>
      </svg>
      <div>
        <strong>${this.escapeHtml(errorTitle)}</strong><br>
        ${this.escapeHtml(message)}
        <br><br>
        <small>${this.escapeHtml(compatibilityNote)}</small>
      </div>
    `;

    // Insert after the button's container
    const button = doc.getElementById("floorp-add-extension-btn");
    if (button?.parentElement) {
      button.parentElement.insertAdjacentElement("afterend", notice);
    }

    // Auto-remove after timeout
    setTimeout(() => {
      notice.remove();
    }, ERROR_NOTICE_TIMEOUT);
  }

  /**
   * Escape HTML special characters to prevent XSS
   */
  private escapeHtml(text: string): string {
    const div = this.document?.createElement("div");
    if (div) {
      div.textContent = text;
      return String(div.innerHTML);
    }
    return text.replace(/[&<>"']/g, (char) => {
      const escapeMap: Record<string, string> = {
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#039;",
      };
      return escapeMap[char] || char;
    });
  }

  /**
   * Handle messages from parent actor
   */
  receiveMessage(message: { name: string; data?: unknown }): unknown {
    switch (message.name) {
      case "CWS:InstallProgress":
        // Handle installation progress updates
        console.log("[NRChromeWebStore] Install progress:", message.data);
        break;

      case "CWS:GetExtensionId":
        return { extensionId: this.currentExtensionId };
    }
    return null;
  }

  /**
   * Clean up when the actor is destroyed
   */
  didDestroy(): void {
    // Stop the main loop
    if (this.mainLoopInterval) {
      clearTimeout(this.mainLoopInterval);
      this.mainLoopInterval = null;
    }
  }
}
