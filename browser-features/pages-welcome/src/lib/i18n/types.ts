export interface WelcomeI18nBridge {
  getOperatingSystemLocale(): Promise<string>;
  normalizeLocale(locale: string): Promise<string>;
}
