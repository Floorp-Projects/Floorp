/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * NRChromeWebStoreChild - Content process actor for Chrome Web Store integration
 *
 * Injects a fixed top banner on Chrome Web Store extension pages allowing
 * users to install Chrome extensions directly into Floorp. The banner is
 * completely self-contained and does not depend on CWS DOM structure.
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

/** Button state reset delay in milliseconds */
const BUTTON_RESET_DELAY = 3000;

/** Error notice auto-hide delay in milliseconds */
const ERROR_NOTICE_TIMEOUT = 10000;

/** Button states */
type ButtonState = "default" | "installing" | "success" | "error" | "installed";

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
  [CWS_I18N_KEYS.banner.message]: "You can install this Chrome extension in Floorp",
  [CWS_I18N_KEYS.banner.messageWithName]: 'You can install "{{name}}" in Floorp',
  [CWS_I18N_KEYS.button.addToFloorp]: "Add to Floorp",
  [CWS_I18N_KEYS.button.installing]: "Installing...",
  [CWS_I18N_KEYS.button.success]: "Added!",
  [CWS_I18N_KEYS.button.error]: "Error occurred",
  [CWS_I18N_KEYS.button.installed]: "Installed",
  [CWS_I18N_KEYS.button.close]: "Close",
  [CWS_I18N_KEYS.error.title]: "Installation Error",
  [CWS_I18N_KEYS.error.compatibilityNote]:
    "This extension may not be compatible with Firefox/Floorp.",
  [CWS_I18N_KEYS.error.installFailed]: "Installation failed",
};

// =============================================================================
// Floorp Logo (64px PNG as data URI)
// =============================================================================

const FLOORP_LOGO_DATA_URI =
  "data:image/png;base64,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";

// =============================================================================
// SVG Path Constants
// =============================================================================

/** SVG path data for the plus icon (default state) */
const SVG_PATH_PLUS = "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z";

/** SVG path data for the checkmark icon (success/installed state) */
const SVG_PATH_CHECK = "M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z";

/** SVG path data for the error icon */
const SVG_PATH_ERROR = "M12 2C6.47 2 2 6.47 2 12s4.47 10 10 10 10-4.47 10-10S17.53 2 12 2zm5 13.59L15.59 17 12 13.41 8.41 17 7 15.59 10.59 12 7 8.41 8.41 7 12 10.59 15.59 7 17 8.41 13.41 12 17 15.59z";

/** SVG path data for the info/warning icon */
const SVG_PATH_INFO = "M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z";

/** SVG path data for the close/X icon */
const SVG_PATH_CLOSE = "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z";

// =============================================================================
// Main Class
// =============================================================================

export class NRChromeWebStoreChild extends JSWindowActorChild {
  private bannerInjected = false;
  private currentExtensionId: string | null = null;
  private installInProgress = false;
  private lastUrl: string | null = null;
  private urlPollingTimer: number | null = null;
  private nameUpdateTimer: number | null = null;
  private experimentEnabled: boolean | null = null;
  private isInstalled = false;

  /**
   * Get a translation for a key, using cache or default values
   */
  private t(key: string, vars?: Record<string, string | number>): string {
    let result = translationCache[key] || DEFAULT_TRANSLATIONS[key] || key;

    if (vars) {
      for (const [k, v] of Object.entries(vars)) {
        result = result.replace(new RegExp(`\\{\\{${k}\\}\\}`, "g"), String(v));
      }
    }
    return result;
  }

  /**
   * Create an SVG element with the given path
   */
  private createSvgElement(viewBox: string, pathData: string, className?: string): SVGSVGElement {
    const doc = this.document!;
    const svg = doc.createElementNS("http://www.w3.org/2000/svg", "svg") as unknown as SVGSVGElement;
    svg.setAttribute("viewBox", viewBox);
    svg.setAttribute("fill", "currentColor");
    if (className) {
      svg.setAttribute("class", className);
    }

    const path = doc.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", pathData);
    svg.appendChild(path);

    return svg;
  }

  /**
   * Create a button content element with icon and text
   */
  private createButtonContent(iconPathData: string, text: string): DocumentFragment {
    const doc = this.document!;
    const fragment = doc.createDocumentFragment();

    const svg = this.createSvgElement("0 0 24 24", iconPathData);
    svg.style.cssText = "width:20px;height:20px";
    fragment.appendChild(svg);

    const span = doc.createElement("span");
    span.textContent = text;
    fragment.appendChild(span);

    return fragment;
  }

  /**
   * Create a spinner element for loading state
   */
  private createSpinner(): HTMLElement {
    const doc = this.document!;
    const spinner = doc.createElement("div");
    spinner.style.cssText =
      "width:16px;height:16px;border:2px solid rgba(255,255,255,0.3);" +
      "border-top-color:white;border-radius:50%;animation:floorp-spin 0.8s linear infinite";
    return spinner;
  }

  /**
   * Initialize translation cache by requesting from parent process
   */
  private async initTranslations(retryCount = 0): Promise<void> {
    if (translationCacheInitialized) return;

    const MAX_RETRIES = 5;
    const RETRY_DELAY = 200;

    try {
      const translations = (await this.sendQuery("CWS:GetTranslations", {
        keys: [
          CWS_I18N_KEYS.banner.message,
          CWS_I18N_KEYS.banner.messageWithName,
          CWS_I18N_KEYS.button.addToFloorp,
          CWS_I18N_KEYS.button.installing,
          CWS_I18N_KEYS.button.success,
          CWS_I18N_KEYS.button.error,
          CWS_I18N_KEYS.button.installed,
          CWS_I18N_KEYS.button.close,
          CWS_I18N_KEYS.error.title,
          CWS_I18N_KEYS.error.compatibilityNote,
          CWS_I18N_KEYS.error.installFailed,
        ],
      })) as Record<string, string> | null;

      if (translations) {
        const firstKey = CWS_I18N_KEYS.button.addToFloorp;
        if (translations[firstKey] && translations[firstKey] !== firstKey) {
          translationCache = { ...translationCache, ...translations };
          translationCacheInitialized = true;
        } else if (retryCount < MAX_RETRIES) {
          await new Promise((resolve) => setTimeout(resolve, RETRY_DELAY));
          return this.initTranslations(retryCount + 1);
        }
      }
    } catch {
      if (retryCount < MAX_RETRIES) {
        await new Promise((resolve) => setTimeout(resolve, RETRY_DELAY));
        return this.initTranslations(retryCount + 1);
      }
    }
  }

  /**
   * Check if the Chrome Web Store experiment is enabled
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
      return enabled;
    } catch {
      this.experimentEnabled = false;
      return false;
    }
  }

  /**
   * Check if the extension is already installed
   */
  private async checkInstallationStatus(extensionId: string): Promise<void> {
    try {
      const result = (await this.sendQuery("CWS:CheckAddonInstalled", {
        extensionId,
      })) as { installed: boolean };
      this.isInstalled = result.installed;

      if (this.bannerInjected && this.isInstalled) {
        const doc = this.document;
        if (doc) {
          const button = doc.getElementById("floorp-add-extension-btn");
          if (button) {
            this.updateButtonState(button, "installed");
          }
        }
      }
    } catch {
      this.isInstalled = false;
    }
  }

  /**
   * Called when the actor is created
   */
  actorCreated(): void {
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
    this.lastUrl = url ?? null;

    if (!url || !isChromeWebStoreUrl(url)) {
      return;
    }

    this.checkExperimentEnabled().then((enabled) => {
      if (!enabled) {
        return;
      }

      this.injectStyles();
      this.setupSPANavigationDetection();

      if (this.isChromeWebStoreExtensionPage(url)) {
        this.injectBanner();
      }
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

    this.lastUrl = url ?? null;

    // Reset state for new page
    this.bannerInjected = false;
    this.currentExtensionId = null;
    this.installInProgress = false;
    this.isInstalled = false;

    this.removeExistingBanner();

    if (url && this.isChromeWebStoreExtensionPage(url)) {
      this.injectBanner();
    }
  }

  /**
   * Set up SPA navigation detection using simple URL polling.
   * This is the most reliable approach that works across all contexts.
   */
  private setupSPANavigationDetection(): void {
    const pollUrl = () => {
      const currentUrl = this.document?.location?.href;
      if (currentUrl && currentUrl !== this.lastUrl) {
        this.onUrlChange();
      }
      this.urlPollingTimer = setTimeout(pollUrl, 500) as unknown as number;
    };
    this.urlPollingTimer = setTimeout(pollUrl, 500) as unknown as number;
  }

  /**
   * Check if URL is a Chrome Web Store extension detail page
   */
  private isChromeWebStoreExtensionPage(url: string): boolean {
    return extractExtensionId(url) !== null;
  }

  /**
   * Inject the fixed top banner into the page
   */
  private injectBanner(): void {
    if (this.bannerInjected) return;

    const doc = this.document;
    if (!doc?.body) return;

    const url = doc.location?.href;
    if (!url) return;

    const extensionId = extractExtensionId(url);
    if (!extensionId) return;

    this.currentExtensionId = extensionId;
    this.checkInstallationStatus(extensionId).catch(() => {});

    // Create banner immediately — name may not be available yet on SPA navigation
    const banner = this.createBanner("");
    doc.body.insertBefore(banner, doc.body.firstChild);

    // Spacer to prevent content from being hidden behind the fixed banner
    const spacer = doc.createElement("div");
    spacer.id = "floorp-cws-banner-spacer";
    spacer.style.height = "66px";
    doc.body.insertBefore(spacer, banner.nextSibling);

    this.bannerInjected = true;

    // Start watching for the extension name to appear in the DOM
    this.startNameUpdatePolling();
  }

  /**
   * Poll for the extension name to appear in the DOM and update the banner.
   * After SPA navigation, the h1 may not be updated yet.
   */
  private startNameUpdatePolling(): void {
    this.stopNameUpdatePolling();

    const MAX_ATTEMPTS = 20;
    let attempts = 0;

    const tryUpdate = () => {
      attempts++;
      const name = this.extractExtensionName();
      if (name) {
        this.updateBannerName(name);
        return;
      }
      if (attempts < MAX_ATTEMPTS) {
        this.nameUpdateTimer = setTimeout(tryUpdate, 300) as unknown as number;
      }
    };

    this.nameUpdateTimer = setTimeout(tryUpdate, 300) as unknown as number;
  }

  private stopNameUpdatePolling(): void {
    if (this.nameUpdateTimer) {
      clearTimeout(this.nameUpdateTimer);
      this.nameUpdateTimer = null;
    }
  }

  /**
   * Update the banner text with the extension name
   */
  private updateBannerName(name: string): void {
    const doc = this.document;
    if (!doc) return;

    const textSpan = doc.querySelector("#floorp-cws-banner .floorp-cws-banner__text span:last-child");
    if (!textSpan) return;

    const messageText = this.t(CWS_I18N_KEYS.banner.messageWithName, { name });
    textSpan.textContent = messageText;
  }

  /**
   * Extract extension name from the page (best-effort, returns empty string if not found)
   */
  private extractExtensionName(): string {
    const doc = this.document;
    if (!doc) return "";

    const h1 = doc.querySelector("h1");
    return h1?.textContent?.trim() || "";
  }

  /**
   * Create the fixed top banner element
   */
  private createBanner(extensionName: string): HTMLElement {
    const doc = this.document!;

    const banner = doc.createElement("div");
    banner.id = "floorp-cws-banner";
    banner.style.cssText =
      "position:fixed;top:0;left:0;right:0;z-index:9999;width:100%;" +
      "background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);" +
      "color:#e0e0e0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;" +
      "font-size:16px;border-bottom:3px solid #0b57d0;" +
      "box-shadow:0 2px 12px rgba(0,0,0,0.4);" +
      "animation:floorp-banner-slide-down 0.3s ease-out";

    const content = doc.createElement("div");
    content.style.cssText =
      "display:flex;align-items:center;justify-content:space-between;" +
      "max-width:1200px;margin:0 auto;padding:12px 24px;gap:20px";

    // Text section
    const textWrapper = doc.createElement("div");
    textWrapper.style.cssText =
      "display:flex;align-items:center;gap:12px;flex:1;min-width:0";

    // Floorp logo
    const logoImg = doc.createElement("img");
    logoImg.src = FLOORP_LOGO_DATA_URI;
    logoImg.alt = "Floorp";
    logoImg.style.cssText = "width:32px;height:32px;flex-shrink:0";
    textWrapper.appendChild(logoImg);

    // Banner message
    const messageText = extensionName
      ? this.t(CWS_I18N_KEYS.banner.messageWithName, { name: extensionName })
      : this.t(CWS_I18N_KEYS.banner.message);

    const textSpan = doc.createElement("span");
    textSpan.textContent = messageText;
    textSpan.style.cssText = "font-size:16px;font-weight:500";
    textWrapper.appendChild(textSpan);

    content.appendChild(textWrapper);

    // Install button
    const button = doc.createElement("button");
    button.id = "floorp-add-extension-btn";
    button.style.cssText =
      "display:inline-flex;align-items:center;justify-content:center;gap:8px;" +
      "padding:0 28px;height:40px;background:#0b57d0;color:white;border:none;" +
      "border-radius:20px;font-size:15px;font-weight:600;cursor:pointer;" +
      "transition:all 0.2s ease;font-family:inherit;white-space:nowrap;flex-shrink:0";

    if (this.isInstalled) {
      this.updateButtonState(button, "installed");
    } else {
      const buttonText = this.t(CWS_I18N_KEYS.button.addToFloorp);
      button.appendChild(this.createButtonContent(SVG_PATH_PLUS, buttonText));
    }

    button.addEventListener("click", () => this.handleInstallClick());
    content.appendChild(button);

    banner.appendChild(content);
    return banner;
  }

  /**
   * Remove the existing banner
   */
  private removeExistingBanner(): void {
    this.stopNameUpdatePolling();

    const doc = this.document;
    if (!doc) return;

    const existing = doc.getElementById("floorp-cws-banner");
    if (existing) {
      existing.remove();
    }

    const spacer = doc.getElementById("floorp-cws-banner-spacer");
    if (spacer) {
      spacer.remove();
    }

    const existingError = doc.getElementById("floorp-cws-error");
    if (existingError) {
      existingError.remove();
    }
  }

  /**
   * Inject CSS styles for the banner and button
   */
  private injectStyles(): void {
    const doc = this.document;
    if (!doc) return;

    const existing = doc.getElementById("floorp-cws-styles");
    if (existing) return;

    const style = doc.createElement("style");
    style.id = "floorp-cws-styles";
    style.textContent = `
      @keyframes floorp-banner-slide-down {
        from { transform: translateY(-100%); opacity: 0; }
        to { transform: translateY(0); opacity: 1; }
      }

      @keyframes floorp-spin {
        to { transform: rotate(360deg); }
      }

      @keyframes floorp-slide-in {
        from { transform: translateX(100%); opacity: 0; }
        to { transform: translateX(0); opacity: 1; }
      }
    `;

    (doc.head ?? doc.documentElement)?.appendChild(style);
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
      this.updateButtonState(button, "installing");

      const metadata = this.extractExtensionMetadata();

      const result = (await this.sendQuery("CWS:InstallExtension", {
        extensionId: this.currentExtensionId,
        metadata,
      })) as { success: boolean; error?: string };

      if (result.success) {
        this.updateButtonState(button, "success");
        this.isInstalled = true;
      } else {
        this.updateButtonState(button, "error");
        this.showError(
          result.error || this.t(CWS_I18N_KEYS.error.installFailed),
        );
      }
    } catch (error) {
      this.updateButtonState(button, "error");
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      this.showError(errorMessage);
    } finally {
      setTimeout(() => {
        if (this.isInstalled) {
          this.updateButtonState(button, "installed");
        } else {
          this.updateButtonState(button, "default");
        }
        this.installInProgress = false;
      }, BUTTON_RESET_DELAY);
    }
  }

  /**
   * Update the button visual state
   */
  private updateButtonState(button: Element | null, state: ButtonState): void {
    if (!button) return;

    const btnStyle = (button as HTMLElement).style;

    while (button.firstChild) {
      button.removeChild(button.firstChild);
    }

    const baseStyle =
      "display:inline-flex;align-items:center;justify-content:center;gap:8px;" +
      "padding:0 28px;height:40px;border:none;border-radius:20px;" +
      "font-size:15px;font-weight:600;font-family:inherit;white-space:nowrap;transition:all 0.2s ease";

    switch (state) {
      case "installing": {
        const installingText = this.t(CWS_I18N_KEYS.button.installing);
        btnStyle.cssText = baseStyle +
          ";background:linear-gradient(135deg,#f59e0b 0%,#d97706 100%);color:white;" +
          "cursor:progress;box-shadow:0 4px 15px rgba(245,158,11,0.4)";
        const spinner = this.createSpinner();
        button.appendChild(spinner);
        const span = this.document!.createElement("span");
        span.textContent = installingText;
        button.appendChild(span);
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      case "success": {
        const successText = this.t(CWS_I18N_KEYS.button.success);
        btnStyle.cssText = baseStyle +
          ";background:linear-gradient(135deg,#10b981 0%,#059669 100%);color:white;" +
          "cursor:default;box-shadow:0 4px 15px rgba(16,185,129,0.4)";
        button.appendChild(this.createButtonContent(SVG_PATH_CHECK, successText));
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      case "installed": {
        const installedText = this.t(CWS_I18N_KEYS.button.installed);
        btnStyle.cssText = baseStyle +
          ";background:#4b5563;color:white;cursor:default;box-shadow:none";
        button.appendChild(this.createButtonContent(SVG_PATH_CHECK, installedText));
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      case "error": {
        const errorText = this.t(CWS_I18N_KEYS.button.error);
        btnStyle.cssText = baseStyle +
          ";background:linear-gradient(135deg,#ef4444 0%,#dc2626 100%);color:white;" +
          "cursor:pointer;box-shadow:0 4px 15px rgba(239,68,68,0.4)";
        button.appendChild(this.createButtonContent(SVG_PATH_ERROR, errorText));
        (button as HTMLButtonElement).disabled = true;
        break;
      }

      default: {
        const defaultText = this.t(CWS_I18N_KEYS.button.addToFloorp);
        btnStyle.cssText = baseStyle +
          ";background:#0b57d0;color:white;cursor:pointer;box-shadow:none";
        button.appendChild(this.createButtonContent(SVG_PATH_PLUS, defaultText));
        (button as HTMLButtonElement).disabled = false;
      }
    }
  }

  /**
   * Extract extension metadata from the current page (best-effort).
   * The name and icon are extracted from h1/img if available.
   */
  private extractExtensionMetadata(): ExtensionMetadata {
    const doc = this.document;
    const metadata: MutableExtensionMetadata = {
      id: this.currentExtensionId || "",
      name: "",
    };

    if (!doc) return metadata;

    // Extension name from h1 (best effort)
    const h1 = doc.querySelector("h1");
    if (h1?.textContent) {
      metadata.name = h1.textContent.trim();
    }

    // Icon from img near h1 (best effort)
    if (h1) {
      const section = h1.closest("section");
      const logoImg = section?.querySelector("img") as HTMLImageElement | null;
      if (logoImg?.src) {
        metadata.iconUrl = logoImg.src;
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

    const existing = doc.getElementById("floorp-cws-error");
    if (existing) {
      existing.remove();
    }

    const errorTitle = this.t(CWS_I18N_KEYS.error.title);
    const compatibilityNote = this.t(CWS_I18N_KEYS.error.compatibilityNote);

    const notice = doc.createElement("div");
    notice.id = "floorp-cws-error";
    notice.style.cssText =
      "position:fixed;top:20px;right:20px;z-index:9999;" +
      "display:flex;align-items:flex-start;gap:12px;margin:0;padding:16px;" +
      "background:#fee2e2;border:1px solid #fca5a5;border-radius:12px;" +
      "font-size:13px;line-height:1.5;color:#991b1b;" +
      "box-shadow:0 4px 12px rgba(0,0,0,0.15);max-width:400px;" +
      "animation:floorp-slide-in 0.3s ease-out";

    const errorIcon = this.createSvgElement("0 0 24 24", SVG_PATH_INFO);
    errorIcon.style.cssText = "flex-shrink:0;width:20px;height:20px";
    notice.appendChild(errorIcon);

    const messageDiv = doc.createElement("div");
    messageDiv.style.cssText = "flex: 1";

    const titleStrong = doc.createElement("strong");
    titleStrong.textContent = errorTitle;
    messageDiv.appendChild(titleStrong);

    messageDiv.appendChild(doc.createElement("br"));
    messageDiv.appendChild(doc.createTextNode(message));
    messageDiv.appendChild(doc.createElement("br"));
    messageDiv.appendChild(doc.createElement("br"));

    const small = doc.createElement("small");
    small.textContent = compatibilityNote;
    messageDiv.appendChild(small);

    notice.appendChild(messageDiv);

    const closeBtn = doc.createElement("button");
    closeBtn.style.cssText =
      "background:none;border:none;cursor:pointer;padding:0;color:inherit;opacity:0.7;margin-left:8px";
    const closeLabel = this.t(CWS_I18N_KEYS.button.close);
    closeBtn.setAttribute("aria-label", closeLabel);
    closeBtn.setAttribute("title", closeLabel);
    const closeIcon = this.createSvgElement("0 0 24 24", SVG_PATH_CLOSE);
    closeIcon.setAttribute("width", "16");
    closeIcon.setAttribute("height", "16");
    closeBtn.appendChild(closeIcon);
    closeBtn.addEventListener("click", () => notice.remove());
    notice.appendChild(closeBtn);

    if (doc.body) {
      doc.body.appendChild(notice);
    } else if (doc.documentElement) {
      doc.documentElement.appendChild(notice);
    }

    setTimeout(() => {
      if (notice.isConnected) {
        notice.remove();
      }
    }, ERROR_NOTICE_TIMEOUT);
  }

  /**
   * Handle messages from parent actor
   */
  receiveMessage(message: { name: string; data?: unknown }): unknown {
    switch (message.name) {
      case "CWS:InstallProgress":
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
    this.stopNameUpdatePolling();
    if (this.urlPollingTimer) {
      clearTimeout(this.urlPollingTimer);
      this.urlPollingTimer = null;
    }
  }
}
