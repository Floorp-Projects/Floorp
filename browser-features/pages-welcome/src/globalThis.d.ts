import type { WelcomeI18nBridge } from "./lib/i18n/types.ts";

declare global {
  var NRI18n: WelcomeI18nBridge;
  interface Window {
    NRGetLocaleInfo: (callback: (localeInfo: string) => void) => void;
    NRSetAppLocale: (
      locale: string,
      callback: (response: string) => void,
    ) => void;
    NRInstallLangPack: (
      // deno-lint-ignore no-explicit-any
      langPack: any,
      callback: (response: string) => void,
    ) => void;
    NRGetNativeNames: (
      langCodes: string[],
      callback: (localeInfo: string) => void,
    ) => void;
    NRSetDefaultBrowser: (callback: (response: string) => void) => void;
    NRDismissWelcomePage: () => void;
  }
}

export {};
