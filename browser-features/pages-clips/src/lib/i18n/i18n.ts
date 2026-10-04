import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";

const translations = import.meta.glob("./locales/*.json", {
  eager: true,
  import: "default",
});

// Convert glob imports to the format expected by i18next
const modules: Record<string, Record<string, object>> = {};
const availableLocales: string[] = [];
for (const [path, content] of Object.entries(translations)) {
  const lng = path.match(/locales\/(.*)\.json/)![1];
  modules[lng] = {
    translations: content as object,
  };
  availableLocales.push(lng);
}

// Map short language codes (e.g. "ja") to full locale tags (e.g. "ja-JP").
// navigator.language often returns short codes, but locale files use full tags.
// Prefer non-mac variants (ja-JP over ja-JP-mac).
function resolveLocale(lng: string): string {
  if (lng in modules) return lng;
  return (
    availableLocales.find((l) => l.startsWith(lng + "-") && !l.endsWith("-mac")) ??
    availableLocales.find((l) => l.startsWith(lng + "-")) ??
    lng
  );
}

export async function initI18nextInstance() {
  i18n.use(LanguageDetector).use(initReactI18next);

  try {
    i18n.on("initialized", () => {
      try {
        globalThis.dispatchEvent(new Event("noraneko:i18n-initialized"));
      } catch {
        /* ignore */
      }
    });
  } catch {
    /* ignore */
  }

  await i18n.init({
    debug: false,
    resources: modules,
    defaultNS: "translations",
    ns: ["translations"],
    fallbackLng: "en-US",
    supportedLngs: availableLocales,
    detection: {
      order: ["navigator", "querystring", "htmlTag"],
      caches: [],
      convertDetectedLanguage: resolveLocale,
    },
    interpolation: {
      escapeValue: false,
      defaultVariables: {
        productName: "Floorp",
      },
    },
    react: {
      useSuspense: false,
    },
  });

  return i18n;
}

export default i18n;
