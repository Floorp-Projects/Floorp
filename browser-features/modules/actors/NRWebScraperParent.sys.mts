/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWebScraperParent extends JSWindowActorParent {
  receiveMessage(
    message: {
      name: string;
      data?: Record<string, unknown>;
    },
  ) {
    // Handle translation requests in parent process
    if (message.name === "WebScraper:Translate") {
      try {
        const { I18nUtils } = ChromeUtils.importESModule(
          "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
        );
        const provider = I18nUtils.getTranslationProvider();
        if (provider && typeof provider.t === "function") {
          const key = message.data?.key as string;
          const vars = (message.data?.vars as Record<string, string | number>) || {};
          const result = provider.t(key, vars);
          if (Array.isArray(result)) {
            return result
              .map((entry) =>
                typeof entry === "string" ? entry : String(entry),
              )
              .join(" ");
          }
          if (typeof result === "string" && result.trim().length > 0) {
            return result;
          }
        }
      } catch {
        // ignore translation errors
      }
      return null;
    }
    // Forward all other messages to the child and return the result.
    return this.sendQuery(message.name, message.data);
  }
}
