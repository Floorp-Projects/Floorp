/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRI18nParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "I18n:GetConstants": {
        const { AppConstants } = ChromeUtils.importESModule(
          "resource://gre/modules/AppConstants.sys.mjs",
        );

        const consts = JSON.stringify(AppConstants);
        return consts;
      }

      // Child asks parent to register a listener. We'll import I18n-Utils
      // in the parent process and forward locale changes to the child.
      case "I18n:RegisterListener": {
        try {
          const { I18nUtils } = ChromeUtils.importESModule(
            "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
          );

          // Ensure we only attach one observer per actor instance
          if (!this._i18nObserver) {
            this._i18nObserver = (newLocale) => {
              this.sendAsyncMessage("I18n:LocaleChanged", newLocale);
            };
            I18nUtils.addLocaleChangeListener(this._i18nObserver);
          }
          // Acknowledge registration
          this.sendAsyncMessage("I18n:RegisterListener:Ack", null);
        } catch (e) {
          Cu.reportError(e);
          this.sendAsyncMessage("I18n:RegisterListener:Error", String(e));
        }
        break;
      }

      case "I18n:UnregisterListener": {
        try {
          const { I18nUtils } = ChromeUtils.importESModule(
            "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
          );
          if (this._i18nObserver) {
            I18nUtils.removeLocaleChangeListener(this._i18nObserver);
            this._i18nObserver = null;
          }
          this.sendAsyncMessage("I18n:UnregisterListener:Ack", null);
        } catch (e) {
          Cu.reportError(e);
          this.sendAsyncMessage("I18n:UnregisterListener:Error", String(e));
        }
        break;
      }

      // Return the current primary mapped locale string
      case "I18n:GetPrimaryLocale": {
        try {
          const { I18nUtils } = ChromeUtils.importESModule(
            "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
          );
          const locale = I18nUtils.getPrimaryBrowserLocaleMapped();
          return locale;
        } catch (e) {
          Cu.reportError(e);
          return String(e);
        }
      }
    }
  }

  // Per-actor instance property to hold the observer callback
  _i18nObserver: ((newLocale: string) => void) | null = null;
}
