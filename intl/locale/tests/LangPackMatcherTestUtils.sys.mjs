/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

import { LangPackMatcher } from "resource://gre/modules/LangPackMatcher.sys.mjs";

/**
 * LangPackMatcher.jsm calls out to to the addons store, which involves network requests.
 * Other tests create a fake addons server, and install mock XPIs. At the time of this
 * writing that infrastructure is not available for mochitests.
 *
 * Instead, this test mocks out APIs that have a side-effect, so the addons of the browser
 * are never modified.
 *
 * The calls to get the app's locale and system's locale are also mocked so that the
 * different language mismatch scenarios can be run through.
 *
 * The locales are BCP 47 identifiers:
 *
 * @param {{
 *   sandbox: SinonSandbox,
 *   systemLocale: string,
 *   appLocale, string,
 * }}
 */
export function getAddonAndLocalAPIsMocker(testScope, sandbox) {
  const { info } = testScope;
  return function mockAddonAndLocaleAPIs({ systemLocale, appLocale }) {
    info("Mocking LangPackMatcher.jsm APIs");

    let resolveLangPacks;
    const langPackPromise = new Promise(resolve => {
      resolveLangPacks = availableLangpacks => {
        info(
          `Resolving which langpacks are available for download: ${JSON.stringify(
            availableLangpacks
          )}`
        );
        resolve(
          availableLangpacks.map(locale => ({
            guid: `langpack-${locale}@firefox.mozilla.org`,
            type: "language",
            hash: locale,
            target_locale: locale,
            current_compatible_version: {
              files: [
                {
                  platform: "all",
                  url: `http://example.com/${locale}.langpack.xpi`,
                },
              ],
            },
          }))
        );
      };
    });

    let resolveInstaller;
    const installerPromise = new Promise(resolve => {
      resolveInstaller = () => {
        info("LangPack install finished.");
        resolve();
      };
    });

    const { mockable } = LangPackMatcher;
    if (appLocale) {
      const availableLocales = [appLocale];
      if (appLocale !== "en-US") {
        // Ensure the fallback behavior is accurately simulated for Firefox.
        availableLocales.push("en-US");
      }
      sandbox
        .stub(mockable, "getAvailableLocalesIncludingFallback")
        .returns(availableLocales);
      sandbox.stub(mockable, "getDefaultLocale").returns(appLocale);
      sandbox.stub(mockable, "getAppLocaleAsBCP47").returns(appLocale);
      sandbox.stub(mockable, "getLastFallbackLocale").returns("en-US");
    }
    if (systemLocale) {
      sandbox.stub(mockable, "getSystemLocale").returns(systemLocale);
    }

    sandbox.stub(mockable, "getAvailableLangpacks").callsFake(() => {
      info("Requesting which langpacks are available for download");
      return langPackPromise;
    });

    sandbox.stub(mockable, "installLangPack").callsFake(langPack => {
      info(`LangPack install started, but pending: ${langPack.target_locale}`);
      return installerPromise;
    });

    sandbox.stub(mockable, "setRequestedAppLocales").callsFake(locales => {
      info(
        `Changing the browser's requested locales to: ${JSON.stringify(
          locales
        )}`
      );
    });

    return {
      /**
       * Resolves the addons API call with available langpacks. Call with a list
       * of BCP 47 identifiers.
       *
       * @type {(availableLangpacks: string[]) => {}}
       */
      resolveLangPacks,

      /**
       * Resolves the pending call to install a langpack.
       *
       * @type {() => {}}
       */
      resolveInstaller,

      /**
       * The mocked APIs.
       */
      mockable,
    };
  };
}
