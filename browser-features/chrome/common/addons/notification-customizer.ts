/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import { createSignal, type Accessor } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import type { ChromeWebStoreInstallInfo } from "./types";

/**
 * Type definitions for i18n text values
 */
type I18nTextValues = {
  chromeExtensionMessage: string;
  chromeExtensionPrompt: string;
  defaultExtensionName: string;
  chromeBadge: string;
  compatibilityWarning: string;
};

/**
 * Translation keys mapping
 */
const translationKeys = {
  chromeExtensionMessage: "addons.notification.chromeExtensionMessage",
  chromeExtensionPrompt: "addons.notification.chromeExtensionPrompt",
  defaultExtensionName: "addons.notification.defaultExtensionName",
  chromeBadge: "addons.notification.chromeBadge",
  compatibilityWarning: "addons.notification.compatibilityWarning",
} as const;

/**
 * Helper function to get translated text with type safety
 */
const t = (key: string, options?: object): string => {
  return (i18next.t as (key: string, options?: object) => string)(key, options);
};

/**
 * Get all translated texts for addon notifications
 */
const getTranslatedTexts = (): I18nTextValues => {
  return {
    chromeExtensionMessage: t(translationKeys.chromeExtensionMessage, {
      name: "{{name}}",
    }),
    chromeExtensionPrompt: t(translationKeys.chromeExtensionPrompt, {
      name: "{{name}}",
    }),
    defaultExtensionName: t(translationKeys.defaultExtensionName),
    chromeBadge: t(translationKeys.chromeBadge),
    compatibilityWarning: t(translationKeys.compatibilityWarning),
  };
};

/**
 * Get the Chrome extension message with name interpolation
 */
const getChromeExtensionMessage = (name: string): string => {
  return t(translationKeys.chromeExtensionMessage, { name });
};

/**
 * Get the Chrome extension prompt with name interpolation
 */
const getChromeExtensionPrompt = (name: string): string => {
  return t(translationKeys.chromeExtensionPrompt, { name });
};

/**
 * Utility class for customizing addon installation notifications
 * for Chrome Web Store extensions
 */
export class NotificationCustomizer {
  private originalDescriptionChildren: Node[] | null = null;
  private originalIntroDisplay: string | null = null;
  private texts: Accessor<I18nTextValues>;

  constructor() {
    // Initialize with default texts
    let textsAccessor: Accessor<I18nTextValues> = () => getTranslatedTexts();

    createRootHMR(
      () => {
        const [texts, setTexts] =
          createSignal<I18nTextValues>(getTranslatedTexts());
        textsAccessor = texts;

        // Set up locale change observer for reactive updates
        addI18nObserver(() => {
          setTexts(getTranslatedTexts());
        });
      },
      import.meta.hot,
    );

    this.texts = textsAccessor;
  }

  /**
   * Customize the notification content for Chrome extensions
   */
  customizeNotificationContent(cwsInfo: ChromeWebStoreInstallInfo): void {
    const notification = document.getElementById(
      "addon-install-confirmation-notification",
    );
    if (!notification) {
      return;
    }

    const texts = this.texts();
    const addonName = cwsInfo.name ?? texts.defaultExtensionName;

    const popupNotificationBody = notification.querySelector(
      "popupnotificationcontent",
    );
    if (!popupNotificationBody) {
      return;
    }

    const descriptionContainer = notification.querySelector(
      ".popup-notification-description",
    );
    if (descriptionContainer) {
      let messageElement = descriptionContainer.querySelector(
        ".chrome-web-store-message",
      ) as HTMLElement | null;
      if (!messageElement) {
        messageElement = document.createElement("span");
        messageElement.className = "chrome-web-store-message";
        descriptionContainer.textContent = "";
        descriptionContainer.appendChild(messageElement);
      }
      messageElement.textContent = getChromeExtensionMessage(addonName);
    }

    this.addChromeExtensionBadge(notification);
    this.addCompatibilityWarning(notification, popupNotificationBody);
  }

  /**
   * Customize the webextension permission prompt for Chrome extensions
   */
  customizeWebExtPermissionPrompt(cwsInfo: ChromeWebStoreInstallInfo): void {
    const notification = document.getElementById(
      "addon-webext-permissions-notification",
    );

    if (!notification) {
      return;
    }

    const texts = this.texts();
    const addonName = cwsInfo.name ?? texts.defaultExtensionName;

    const popupNotificationBody = notification.querySelector(
      "popupnotificationcontent",
    );

    if (!popupNotificationBody) {
      return;
    }

    const descriptionContainer = notification.querySelector(
      ".popup-notification-description",
    );

    if (descriptionContainer) {
      if (this.originalDescriptionChildren === null) {
        this.originalDescriptionChildren = Array.from(
          descriptionContainer.childNodes,
        ).map((node) => node.cloneNode(true));
      }

      let messageElement = descriptionContainer.querySelector(
        ".chrome-web-store-message",
      ) as HTMLElement | null;
      if (!messageElement) {
        messageElement = document.createElement("span");
        messageElement.className = "chrome-web-store-message";
        descriptionContainer.textContent = "";
        descriptionContainer.appendChild(messageElement);
      }
      const promptText = getChromeExtensionPrompt(addonName);
      messageElement.textContent = promptText;
    }

    const introElement = notification.querySelector(
      "#addon-webext-perm-intro",
    ) as HTMLElement | null;
    if (introElement) {
      if (this.originalIntroDisplay === null) {
        this.originalIntroDisplay =
          introElement.style.getPropertyValue("display") || "";
      }
      introElement.style.setProperty("display", "none");
    }

    this.addCompatibilityWarning(notification, popupNotificationBody);
    notification.setAttribute("data-cws-customized", "true");
  }

  /**
   * Restore the original content of the permission prompt
   */
  restoreOriginalContent(): void {
    const notification = document.getElementById(
      "addon-webext-permissions-notification",
    );
    if (!notification) {
      return;
    }

    if (!notification.hasAttribute("data-cws-customized")) {
      return;
    }

    const descriptionContainer = notification.querySelector(
      ".popup-notification-description",
    );
    if (descriptionContainer && this.originalDescriptionChildren !== null) {
      descriptionContainer.textContent = "";
      for (const child of this.originalDescriptionChildren) {
        descriptionContainer.appendChild(child.cloneNode(true));
      }
      this.originalDescriptionChildren = null;
    }

    const introElement = notification.querySelector(
      "#addon-webext-perm-intro",
    ) as HTMLElement | null;
    if (introElement && this.originalIntroDisplay !== null) {
      introElement.style.setProperty("display", this.originalIntroDisplay);
      this.originalIntroDisplay = null;
    }

    const warning = notification.querySelector(".chrome-extension-warning");
    if (warning) {
      warning.remove();
    }

    notification.removeAttribute("data-cws-customized");
  }

  /**
   * Add a Chrome extension badge to the notification
   */
  private addChromeExtensionBadge(notification: Element): void {
    if (notification.querySelector(".chrome-extension-badge")) {
      return;
    }

    const addonList = document.getElementById(
      "addon-install-confirmation-content",
    );
    if (!addonList) {
      return;
    }

    const texts = this.texts();
    const nameElements = addonList.querySelectorAll(
      ".addon-install-confirmation-name",
    );
    for (const nameElement of nameElements) {
      const badge = document.createXULElement("label") as XULElement;
      badge.setAttribute("value", texts.chromeBadge);
      badge.setAttribute("class", "chrome-extension-badge");
      (badge as XULElement & { style: CSSStyleDeclaration }).style.cssText =
        "color: #4285f4; font-weight: bold; margin-inline-start: 8px; font-size: 0.9em;";
      nameElement.parentElement?.appendChild(badge);
    }
  }

  /**
   * Add a compatibility warning for Chrome extensions
   */
  private addCompatibilityWarning(notification: Element, body: Element): void {
    if (notification.querySelector(".chrome-extension-warning")) {
      return;
    }

    const texts = this.texts();
    const warning = document.createXULElement("description") as XULElement;
    warning.setAttribute("class", "chrome-extension-warning");
    (warning as XULElement & { style: CSSStyleDeclaration }).style.cssText =
      "color: #f9ab00; font-size: 0.85em; margin-top: 8px; padding: 4px 8px; background: rgba(249, 171, 0, 0.1); border-radius: 4px;";
    warning.textContent = texts.compatibilityWarning;

    body.appendChild(warning);
  }
}
