/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Translation helper for NRWebScraperChild
 */

import { FALLBACK_TRANSLATIONS } from "./constants.ts";
import type { WebScraperContext } from "./types.ts";

/**
 * Helper class for translation and localization
 */
export class TranslationHelper {
  constructor(private context: WebScraperContext) {}

  /**
   * Translate a key with optional variables
   */
  async translate(
    key: string,
    vars: Record<string, string | number> = {},
  ): Promise<string> {
    // Try to get translation from parent process via message
    try {
      const result = (await this.context.sendQuery("WebScraper:Translate", {
        key: `browser-chrome:nr-webscraper.${key}`,
        vars,
      })) as string | null | undefined;

      if (result && typeof result === "string" && result.trim().length > 0) {
        return this.formatTemplate(result, vars);
      }
    } catch {
      // Silently fallback to English on translation errors
    }
    // Fallback to English
    return this.formatTemplate(this.getFallbackString(key), vars);
  }

  /**
   * Format a template string with variables
   */
  formatTemplate(
    template: string,
    vars: Record<string, string | number>,
  ): string {
    return template
      .replace(/{{\s*(\w+)\s*}}/g, (_match, name) => {
        const value = vars[name];
        return value !== undefined && value !== null ? String(value) : "";
      })
      .replace(/{\s*(\w+)\s*}/g, (_match, name) => {
        const value = vars[name];
        return value !== undefined && value !== null ? String(value) : "";
      });
  }

  /**
   * Get fallback string for a translation key
   */
  getFallbackString(key: string): string {
    return FALLBACK_TRANSLATIONS[key] || key;
  }

  /**
   * Get action label for display
   */
  async getActionLabel(action: string | undefined): Promise<string> {
    if (!action) {
      return await this.translate("actionLabelHighlight");
    }
    const actionMap: Record<string, string> = {
      highlight: "actionLabelHighlight",
      inspect: "actionLabelInspect",
      fill: "actionLabelFill",
      input: "actionLabelInput",
      click: "actionLabelClick",
      submit: "actionLabelSubmit",
    };
    const key = actionMap[action.toLowerCase()];
    if (key) {
      return await this.translate(key);
    }
    return action;
  }

  /**
   * Truncate a string to a maximum length
   */
  truncate(value: string, limit: number): string {
    if (value.length <= limit) {
      return value;
    }
    return `${value.substring(0, limit)}...`;
  }
}
