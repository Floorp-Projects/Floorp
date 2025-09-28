export type LangPack = {
  hash: string;
  target_locale: string;
  url: string;
};

export type LocaleData = {
  availableLocales: LangPack[];
  installedLocales: string[];
  langPackInfo: LangPack;
  localeInfo: LocaleInfo;
};

export type LocaleInfo = {
  availableLocales: LangPack[];
  installedLocales: string[];
  langPackInfo: LangPack;
  appLocale: {
    baseName: string;
    language: string;
    region: string;
  };
  appLocaleRaw: string;
  canLiveReload: boolean;
  displayNames: {
    systemLanguage: string;
    appLanguage: string;
  };
  matchType: string;
  systemLocale: {
    baseName: string;
    language: string;
    region: string;
  };
  systemLocaleRaw: string;
};
