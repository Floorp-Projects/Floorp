class i18nUtils {
  private static readonly LOCALE_OBSERVER_KEY = "intl:app-locales-changed";
  private get browserRequestedLocales(): string[] {
    return Services.locale.appLocalesAsBCP47 || [this.fallBackLocale];
  }

  private get fallBackLocale(): string {
    return "en-US";
  }

  private get operatingSystemLocale(): string {
    return Services.locale.requestedLocales?.[0] || this.fallBackLocale;
  }

  // Map of language-only codes to preferred BCP47 locales
  private readonly localeMap: Record<string, string> = {
    // Ensure 'ja' maps to 'ja-JP' so Japanese prefers Japan regional variant
    ja: "ja-JP", // --- 拡張例 ---
    de: "de-DE", // ドイツ語 -> ドイツ
    fr: "fr-FR", // フランス語 -> フランス
    it: "it-IT", // イタリア語 -> イタリア
    nl: "nl-NL", // オランダ語 -> オランダ
    pl: "pl-PL", // ポーランド語 -> ポーランド
    ru: "ru-RU", // ロシア語 -> ロシア
    tr: "tr-TR", // トルコ語 -> トルコ
    uk: "uk-UA", // ウクライナ語 -> ウクライナ
    cs: "cs-CZ", // チェコ語 -> チェコ
    da: "da-DK", // デンマーク語 -> デンマーク
    fi: "fi-FI", // フィンランド語 -> フィンランド
    el: "el-GR", // ギリシャ語 -> ギリシャ
    hu: "hu-HU", // ハンガリー語 -> ハンガリー
    ro: "ro-RO", // ルーマニア語 -> ルーマニア
    ko: "ko-KR", // 韓国語 -> 韓国
    id: "id-ID", // インドネシア語 -> インドネシア
    th: "th-TH", // タイ語 -> タイ
    vi: "vi-VN", // ベトナム語 -> ベトナム
    he: "he-IL", // ヘブライ語 -> イスラエル
  };

  // Normalize a single locale string: if it lacks a region, map it when possible
  private mapLocale(locale: string | null | undefined): string {
    if (!locale) return this.fallBackLocale;
    const trimmed = locale.trim();
    if (!trimmed) return this.fallBackLocale;
    // If already a regionalized code like 'en-US' or 'ja-JP', return as-is
    if (trimmed.includes("-")) return trimmed;
    const key = trimmed.toLowerCase();
    // If we have a mapping, use it
    if (this.localeMap[key]) return this.localeMap[key];

    // Fallback: if OS locale has the same language, prefer the OS regionalized locale
    try {
      const os = this.operatingSystemLocale; // e.g. 'ja-JP'
      const osLang = os.split("-")[0].toLowerCase();
      if (osLang === key && os.includes("-")) return os;
    } catch {
      // ignore and fall through
    }

    // Otherwise return the original trimmed value (language-only)
    return trimmed;
  }

  // Return the browser requested locales after mapping/normalization
  public getBrowserRequestedLocalesMapped(): string[] {
    try {
      return this.browserRequestedLocales.map((l) => this.mapLocale(l));
    } catch {
      return [this.fallBackLocale];
    }
  }

  // Return the primary mapped browser locale (first preference)
  public getPrimaryBrowserLocaleMapped(): string {
    const mapped = this.getBrowserRequestedLocalesMapped();
    return mapped.length ? mapped[0] : this.fallBackLocale;
  }

  // Observers/listeners for locale changes. Each listener receives the new locale string.
  private listeners: Array<(newLocale: string) => void> = [];
  private observerBound = this.onLocaleChanged.bind(this);

  constructor() {
    // Register to listen for locale changes when the singleton is created
    try {
      Services.obs.addObserver(
        this.observerBound,
        i18nUtils.LOCALE_OBSERVER_KEY,
      );
    } catch {
      // If registration fails, keep going; listeners can still be added and removed
      // but they won't be triggered by the observer.
    }
  }

  private onLocaleChanged(_subject: unknown, topic: string, _data: string) {
    if (topic !== i18nUtils.LOCALE_OBSERVER_KEY) return;
    // Determine the new primary mapped locale and pass it to listeners
    const newLocale = this.getPrimaryBrowserLocaleMapped();
    // Notify registered listeners with the new locale
    for (const cb of Array.from(this.listeners)) {
      try {
        cb(newLocale);
      } catch {
        // swallow listener errors to avoid breaking other listeners
      }
    }
  }

  // Public API to add/remove listeners
  public addLocaleChangeListener(cb: (newLocale: string) => void): void {
    const isNew = !this.listeners.includes(cb);
    if (isNew) this.listeners.push(cb);
    // Fire immediately so the listener can perform initial initialization
    try {
      cb(this.getPrimaryBrowserLocaleMapped());
    } catch {
      // swallow listener errors to avoid breaking the caller
    }
  }

  public removeLocaleChangeListener(cb: (newLocale: string) => void): void {
    this.listeners = this.listeners.filter((l) => l !== cb);
  }

  public getOperatingSystemLocale(): string {
    // Return the mapped/normalized operating system locale so callers
    // always receive a regionalized BCP47 locale when possible.
    try {
      const os = this.operatingSystemLocale;
      return this.mapLocale(os);
    } catch {
      return this.fallBackLocale;
    }
  }

  public normalizeLocale(locale: string): string {
    return this.mapLocale(locale);
  }

  private static instance: i18nUtils;
  public static getInstance(): i18nUtils {
    if (!i18nUtils.instance) {
      i18nUtils.instance = new i18nUtils();
    }
    return i18nUtils.instance;
  }
}

export const I18nUtils = i18nUtils.getInstance();