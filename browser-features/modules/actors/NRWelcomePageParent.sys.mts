/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Open a URL in a new tab from the browser window. A plain <a target="_blank">
// click inside a system-principal about page (about:welcome) opens about:blank,
// so the page routes external links here instead. See Floorp issue #2787.
// deno-lint-ignore no-explicit-any
function openExternalLinkInBrowser(url: string): boolean {
  try {
    const win = Services.wm.getMostRecentWindow("navigator:browser") as any;
    if (!win || typeof win.openTrustedLinkIn !== "function") {
      return false;
    }
    win.openTrustedLinkIn(url, "tab", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      relatedToCurrent: true,
      // Never let the new content tab inherit the system principal.
      allowInheritPrincipal: false,
    });
    return true;
  } catch (error) {
    console.error("[noraneko] openExternalLinkInBrowser failed", error);
    return false;
  }
}

export class NRWelcomePageParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "WelcomePage:openExternalLink": {
        const data = message.data as { url?: unknown } | undefined;
        const url = typeof data?.url === "string" ? data.url : "";
        if (url) {
          openExternalLinkInBrowser(url);
        }
        break;
      }
      case "WelcomePage:dismiss": {
        const context = this.browsingContext;
        const uri = this.manager.documentURI;
        const isWelcomePage = uri.spec.split(/[?#]/)[0] === "about:welcome" ||
          (uri.schemeIs("chrome") && uri.host === "noraneko-welcome") ||
          (uri.schemeIs("http") && uri.port === 5187 &&
            (uri.host === "localhost" || uri.host === "127.0.0.1"));
        // Only the current top-level welcome document may dismiss its own tab.
        if (
          !isWelcomePage || context.parent ||
          context.currentWindowGlobal !== this.manager
        ) break;
        const browser = context.embedderElement;
        const tabBrowser = (browser?.ownerDocument.defaultView as Window | null)
          ?.gBrowser;
        const tab = browser && tabBrowser?.getTabForBrowser(browser);
        if (!tab || !tabBrowser || tab.closing) break;
        const hasOtherTab = tabBrowser.tabs.some((other) =>
          other !== tab && other.isOpen && !other.hidden
        );
        if (hasOtherTab) {
          // Also prevent window closure if another tab starts closing meanwhile.
          tabBrowser.removeTab(tab, { closeWindowWithLastTab: false });
        } else {
          browser.loadURI(Services.io.newURI("about:newtab"), {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
        }
        break;
      }
      case "WelcomePage:getLocaleInfo": {
        const { LangPackMatcher } = ChromeUtils.importESModule(
          "moz-src:///intl/locale/LangPackMatcher.sys.mjs",
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
          "moz-src:///intl/locale/LangPackMatcher.sys.mjs",
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
          "moz-src:///intl/locale/LangPackMatcher.sys.mjs",
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
          "moz-src:///browser/components/shell/ShellService.sys.mjs",
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
