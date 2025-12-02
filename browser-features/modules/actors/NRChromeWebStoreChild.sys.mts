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
 */

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

interface ExtensionMetadata {
  id: string;
  name: string;
  version?: string;
  description?: string;
  iconUrl?: string;
  author?: string;
}

export class NRChromeWebStoreChild extends JSWindowActorChild {
  private buttonInjected = false;
  private styleInjected = false;
  private currentExtensionId: string | null = null;
  private installInProgress = false;
  private lastUrl: string | null = null;
  private mainLoopInterval: number | null = null;
  private removedRecommendChrome = false;

  // Use a simple interval-based approach like Fire-CWS (100ms)
  private static readonly MAIN_LOOP_INTERVAL = 100;

  actorCreated(): void {
    console.debug(
      "[NRChromeWebStore] Child actor created for:",
      this.contentWindow?.location?.href,
    );
  }

  handleEvent(event: Event): void {
    if (event.type === "DOMContentLoaded") {
      this.onDOMContentLoaded();
    } else if (event.type === "popstate") {
      this.onUrlChange();
    }
  }

  private onDOMContentLoaded(): void {
    const url = this.document?.location?.href;
    console.debug("[NRChromeWebStore] DOMContentLoaded:", url);

    this.lastUrl = url ?? null;

    if (!this.isChromeWebStore(url)) {
      return;
    }

    // Inject styles
    this.injectStyles();

    // Start main loop for button injection and page modifications
    // Following Fire-CWS approach: simple interval-based checking
    this.startMainLoop();
  }

  /**
   * Check if URL is on Chrome Web Store (any page)
   */
  private isChromeWebStore(url?: string): boolean {
    if (!url) return false;
    return (
      url.startsWith("https://chromewebstore.google.com/") ||
      url.startsWith("https://chrome.google.com/webstore/")
    );
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
    this.removedRecommendChrome = false;

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

      // Remove "Recommend Chrome" banner
      this.removeRecommendChromeBanner();

      // Inject button on extension pages
      if (this.isChromeWebStoreExtensionPage(currentUrl)) {
        this.attemptButtonInjection();
      }

      // Continue the loop
      this.mainLoopInterval = setTimeout(
        mainLoop,
        NRChromeWebStoreChild.MAIN_LOOP_INTERVAL,
      ) as unknown as number;
    };

    // Start the loop
    this.mainLoopInterval = setTimeout(
      mainLoop,
      NRChromeWebStoreChild.MAIN_LOOP_INTERVAL,
    ) as unknown as number;
  }

  /**
   * Remove the "Recommend Chrome" promotional banner
   * Following Fire-CWS approach
   */
  private removeRecommendChromeBanner(): void {
    if (this.removedRecommendChrome) return;

    const doc = this.document;
    if (!doc) return;

    // Fire-CWS selector: [aria-labelledby="promo-header"]
    const promoSelectors = [
      '[aria-labelledby="promo-header"]',
      '[class*="promo"]',
      '[class*="chrome-promo"]',
      '[class*="install-chrome"]',
    ];

    for (const selector of promoSelectors) {
      const elements = doc.querySelectorAll(selector);
      for (const elem of elements) {
        // Check if this is actually a Chrome promotion banner
        const text = elem.textContent?.toLowerCase() || "";
        if (
          text.includes("chrome") &&
          (text.includes("download") ||
            text.includes("install") ||
            text.includes("get") ||
            text.includes("ダウンロード") ||
            text.includes("インストール"))
        ) {
          elem.remove();
          this.removedRecommendChrome = true;
          console.debug(
            "[NRChromeWebStore] Removed Chrome recommendation banner",
          );
          return;
        }
      }
    }
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

  private isChromeWebStoreExtensionPage(url?: string): boolean {
    if (!url) return false;

    // Match patterns:
    // - https://chromewebstore.google.com/detail/{name}/{id}
    // - https://chrome.google.com/webstore/detail/{name}/{id}
    const patterns = [
      /^https:\/\/chromewebstore\.google\.com\/detail\/[^/]+\/([a-z]{32})/,
      /^https:\/\/chrome\.google\.com\/webstore\/detail\/[^/]+\/([a-z]{32})/,
    ];

    return patterns.some((pattern) => pattern.test(url));
  }

  private extractExtensionId(url?: string): string | null {
    if (!url) return null;

    const patterns = [
      /^https:\/\/chromewebstore\.google\.com\/detail\/[^/]+\/([a-z]{32})/,
      /^https:\/\/chrome\.google\.com\/webstore\/detail\/[^/]+\/([a-z]{32})/,
    ];

    for (const pattern of patterns) {
      const match = url.match(pattern);
      if (match && match[1]) {
        return match[1];
      }
    }

    return null;
  }

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
        padding: 10px 24px;
        margin: 8px;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        border: none;
        border-radius: 24px;
        font-size: 14px;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.3s ease;
        box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      }

      .floorp-add-button:hover {
        transform: translateY(-2px);
        box-shadow: 0 6px 20px rgba(102, 126, 234, 0.5);
        background: linear-gradient(135deg, #5a6fd6 0%, #6a4190 100%);
      }

      .floorp-add-button:active {
        transform: translateY(0);
        box-shadow: 0 2px 10px rgba(102, 126, 234, 0.3);
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
    `;

    (doc.head ?? doc.documentElement)?.appendChild(style);
    this.styleInjected = true;
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
   * Find the "Add to Chrome" button using multiple detection strategies
   * Inspired by Fire-CWS's approach with jscontroller/jsaction selectors
   */
  private findAddToChromeButton(): Element | null {
    const doc = this.document;
    if (!doc) return null;

    // Strategy 1: Fire-CWS style selector (jscontroller/jsaction based)
    // This is the most reliable selector for Chrome Web Store buttons
    const fireCwsSelectors = [
      "button[jscontroller][jsaction]",
      "button[jscontroller][jsaction][data-id]",
    ];

    for (const selector of fireCwsSelectors) {
      const buttons = doc.querySelectorAll(selector);
      for (const button of buttons) {
        // Check if button text contains "Chrome" or similar
        const text = (button.textContent || "").toLowerCase();
        if (
          (text.includes("chrome") ||
            text.includes("add") ||
            text.includes("追加")) &&
          button.id !== "floorp-add-extension-btn" &&
          this.isElementVisible(button)
        ) {
          return button;
        }
      }
    }

    // Strategy 2: Look for buttons with specific text content
    const addToChromePatterns = [
      "add to chrome",
      "chrome に追加",
      "zu chrome hinzufügen",
      "ajouter à chrome",
      "añadir a chrome",
      "agregar a chrome",
      "adicionar ao chrome",
      "添加至 chrome",
      "добавить в chrome",
      "remove from chrome",
      "chromeから削除",
    ];

    // Get all buttons on the page
    const allButtons = doc.querySelectorAll("button");
    for (const button of allButtons) {
      const text = (button.textContent || "").toLowerCase().trim();
      const ariaLabel = (button.getAttribute("aria-label") || "").toLowerCase();

      for (const pattern of addToChromePatterns) {
        if (text.includes(pattern) || ariaLabel.includes(pattern)) {
          // Verify it's visible and not our own button
          if (
            button.id !== "floorp-add-extension-btn" &&
            this.isElementVisible(button)
          ) {
            return button;
          }
        }
      }
    }

    // Strategy 3: Look for specific data attributes
    const dataSelectors = [
      '[data-test-id="action-button"]',
      "[data-item-id] button[aria-label]",
      '[jsname] button[aria-label*="Chrome"]',
    ];

    for (const selector of dataSelectors) {
      const element = doc.querySelector(selector);
      if (element && this.isElementVisible(element)) {
        return element;
      }
    }

    return null;
  }

  /**
   * Find a fallback container for button injection
   */
  private findFallbackContainer(): Element | null {
    const doc = this.document;
    if (!doc) return null;

    // Look for common container patterns in Chrome Web Store
    const containerSelectors = [
      // Publisher header action area (new CWS)
      'header [class*="action"]',
      '[class*="header"] [class*="button"]',
      // Detail page action containers
      '[class*="detail"] [class*="action"]',
      // Generic patterns
      "main header",
      '[class*="publisher"]',
    ];

    for (const selector of containerSelectors) {
      const elements = doc.querySelectorAll(selector);
      for (const el of elements) {
        // Check if this container has any visible button children
        const hasButtons = el.querySelector("button");
        if (hasButtons && this.isElementVisible(el)) {
          return el;
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

    const extensionId = this.extractExtensionId(locationHref);
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

    const extensionId = this.extractExtensionId(locationHref);
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
    button.innerHTML = `
      <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
        <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z"/>
      </svg>
      <span>Floorp に追加</span>
    `;

    button.addEventListener("click", () => this.handleInstallClick());
    return button;
  }

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
        this.showError(result.error || "インストールに失敗しました");
        console.error("[NRChromeWebStore] Installation failed:", result.error);
      }
    } catch (error) {
      this.updateButtonState(button, "error");
      const errorMessage =
        error instanceof Error ? error.message : String(error);
      this.showError(errorMessage);
      console.error("[NRChromeWebStore] Installation error:", error);
    } finally {
      // Reset button state after 3 seconds
      setTimeout(() => {
        this.updateButtonState(button, "default");
        this.installInProgress = false;
      }, 3000);
    }
  }

  private updateButtonState(
    button: Element | null,
    state: "default" | "installing" | "success" | "error",
  ): void {
    if (!button) return;

    // Remove all state classes
    button.classList.remove(
      "floorp-add-button--installing",
      "floorp-add-button--success",
      "floorp-add-button--error",
    );

    // Update content and class based on state
    switch (state) {
      case "installing":
        button.classList.add("floorp-add-button--installing");
        button.innerHTML = `
          <div class="floorp-add-button__spinner"></div>
          <span>インストール中...</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;

      case "success":
        button.classList.add("floorp-add-button--success");
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/>
          </svg>
          <span>追加しました！</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;

      case "error":
        button.classList.add("floorp-add-button--error");
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C6.47 2 2 6.47 2 12s4.47 10 10 10 10-4.47 10-10S17.53 2 12 2zm5 13.59L15.59 17 12 13.41 8.41 17 7 15.59 10.59 12 7 8.41 8.41 7 12 10.59 15.59 7 17 8.41 13.41 12 17 15.59z"/>
          </svg>
          <span>エラーが発生しました</span>
        `;
        (button as HTMLButtonElement).disabled = true;
        break;

      default:
        button.innerHTML = `
          <svg class="floorp-add-button__icon" viewBox="0 0 24 24" fill="currentColor">
            <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm5 11h-4v4h-2v-4H7v-2h4V7h2v4h4v2z"/>
          </svg>
          <span>Floorp に追加</span>
        `;
        (button as HTMLButtonElement).disabled = false;
    }
  }

  private extractExtensionMetadata(): ExtensionMetadata {
    const doc = this.document;
    const metadata: ExtensionMetadata = {
      id: this.currentExtensionId || "",
      name: "",
    };

    if (!doc) return metadata;

    // Extract extension name
    const nameSelectors = [
      'h1[class*="name"]',
      '[data-test-id="extension-name"]',
      "h1",
      ".e-f-w",
    ];

    for (const selector of nameSelectors) {
      const el = doc.querySelector(selector);
      if (el?.textContent) {
        metadata.name = el.textContent.trim();
        break;
      }
    }

    // Extract version
    const versionSelectors = ['[class*="version"]', ".C-b-p-D-Xe"];

    for (const selector of versionSelectors) {
      const el = doc.querySelector(selector);
      if (el?.textContent) {
        const versionMatch = el.textContent.match(/[\d.]+/);
        if (versionMatch) {
          metadata.version = versionMatch[0];
          break;
        }
      }
    }

    // Extract description
    const descSelectors = ['[class*="description"]', ".C-b-p-j-D"];

    for (const selector of descSelectors) {
      const el = doc.querySelector(selector);
      if (el?.textContent) {
        metadata.description = el.textContent.trim().slice(0, 500);
        break;
      }
    }

    // Extract icon URL
    const iconSelectors = ['img[class*="icon"]', ".e-f-s img", ".Cb-j img"];

    for (const selector of iconSelectors) {
      const el = doc.querySelector(selector) as HTMLImageElement;
      if (el?.src) {
        metadata.iconUrl = el.src;
        break;
      }
    }

    // Extract author
    const authorSelectors = [
      '[class*="publisher"]',
      '[class*="author"]',
      ".e-f-Me",
    ];

    for (const selector of authorSelectors) {
      const el = doc.querySelector(selector);
      if (el?.textContent) {
        metadata.author = el.textContent.trim();
        break;
      }
    }

    return metadata;
  }

  private showError(message: string): void {
    const doc = this.document;
    if (!doc) return;

    // Remove existing error notice
    const existing = doc.getElementById("floorp-cws-error");
    if (existing) {
      existing.remove();
    }

    const notice = doc.createElement("div");
    notice.id = "floorp-cws-error";
    notice.className = "floorp-cws-notice floorp-cws-notice--error";
    notice.innerHTML = `
      <svg class="floorp-cws-notice__icon" viewBox="0 0 24 24" fill="currentColor">
        <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>
      </svg>
      <div>
        <strong>インストールエラー</strong><br>
        ${this.escapeHtml(message)}
        <br><br>
        <small>この拡張機能は Firefox/Floorp と互換性がない可能性があります。</small>
      </div>
    `;

    // Insert after the button's container
    const button = doc.getElementById("floorp-add-extension-btn");
    if (button?.parentElement) {
      button.parentElement.insertAdjacentElement("afterend", notice);
    }

    // Auto-remove after 10 seconds
    setTimeout(() => {
      notice.remove();
    }, 10000);
  }

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

  didDestroy(): void {
    // Stop the main loop
    if (this.mainLoopInterval) {
      clearTimeout(this.mainLoopInterval);
      this.mainLoopInterval = null;
    }
  }
}
