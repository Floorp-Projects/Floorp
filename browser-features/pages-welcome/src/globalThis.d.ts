declare global {
  interface Window {
    NRGetLocaleInfo: (callback: (localeInfo: string) => void) => void;
    NRSetAppLocale: (
      locale: string,
      callback: (response: string) => void,
    ) => void;
    NRInstallLangPack: (
      langPack: any,
      callback: (response: string) => void,
    ) => void;
    NRGetNativeNames: (
      langCodes: string[],
      callback: (localeInfo: string) => void,
    ) => void;
    NRSetDefaultBrowser: (callback: (response: string) => void) => void;
  }
}

export {};
