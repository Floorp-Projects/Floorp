/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRWelcomePageParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "WelcomePage:getLocaleInfo": {
        const { LangPackMatcher } = ChromeUtils.importESModule(
          "resource://gre/modules/LangPackMatcher.sys.mjs",
        );

        const localeInfo = LangPackMatcher.getAppAndSystemLocaleInfo();
        const isUserLocaleSet = Services.prefs.prefHasUserValue(
          "intl.locale.requested",
        );
        const availableLocales = await LangPackMatcher.mockable
          .getAvailableLangpacks();
        const installedLocales = await LangPackMatcher.getAvailableLocales();
        let langPackInfo = null;
        if (localeInfo.matchType !== "match") {
          langPackInfo = await LangPackMatcher
            .negotiateLangPackForLanguageMismatch();
        }
        this.sendAsyncMessage(
          "WelcomePage:localeInfoResponse",
          JSON.stringify({
            localeInfo: {
              ...localeInfo,
              isUserLocaleSet,
            },
            availableLocales,
            installedLocales,
            langPackInfo,
          }),
        );
        break;
      }

      case "WelcomePage:setAppLocale": {
        const { LangPackMatcher } = ChromeUtils.importESModule(
          "resource://gre/modules/LangPackMatcher.sys.mjs",
        );

        const { locale } = message.data;

        if (locale) {
          LangPackMatcher.setRequestedAppLocales([locale]);

          this.sendAsyncMessage(
            "WelcomePage:setAppLocaleResponse",
            JSON.stringify({ success: true, locale }),
          );
        } else {
          this.sendAsyncMessage(
            "WelcomePage:setAppLocaleResponse",
            JSON.stringify({
              success: false,
              error: "No locale specified",
            }),
          );
        }
        break;
      }

      case "WelcomePage:installLangPack": {
        const { LangPackMatcher } = ChromeUtils.importESModule(
          "resource://gre/modules/LangPackMatcher.sys.mjs",
        );

        const { langPack } = message.data;

        if (langPack) {
          try {
            const success = await LangPackMatcher.ensureLangPackInstalled(
              langPack,
            );

            this.sendAsyncMessage(
              "WelcomePage:installLangPackResponse",
              JSON.stringify({
                success,
                locale: langPack.target_locale,
              }),
            );
          } catch (error) {
            this.sendAsyncMessage(
              "WelcomePage:installLangPackResponse",
              JSON.stringify({
                success: false,
                error: String(error),
              }),
            );
          }
        } else {
          this.sendAsyncMessage(
            "WelcomePage:installLangPackResponse",
            JSON.stringify({
              success: false,
              error: "No language pack specified",
            }),
          );
        }
        break;
      }

      case "WelcomePage:getNativeNames": {
        const { MozIntl } = ChromeUtils.importESModule(
          "resource://gre/modules/mozIntl.sys.mjs",
        );

        console.log("WelcomePage:getNativeNames");

        const IntilSupprt = new MozIntl();
        const { langCodes } = message.data;
        const nativeNames = IntilSupprt.getLocaleDisplayNames(
          undefined,
          langCodes,
          { preferNative: true },
        );

        this.sendAsyncMessage(
          "WelcomePage:getNativeNamesResponse",
          JSON.stringify(nativeNames),
        );
        break;
      }

      case "WelcomePage:setDefaultBrowser": {
        const { ShellService } = ChromeUtils.importESModule(
          "resource:///modules/ShellService.sys.mjs",
        );

        await ShellService.setDefaultBrowser();

        this.sendAsyncMessage(
          "WelcomePage:setDefaultBrowserResponse",
          JSON.stringify({ success: true }),
        );
        break;
      }
    }
  }
}
