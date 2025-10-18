import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";

const translations = import.meta.glob("./locales/*.json", {
  eager: true,
  import: "default",
});

// Convert glob imports to the format expected by i18next
const modules: Record<string, Record<string, object>> = {};
for (const [path, content] of Object.entries(translations)) {
  const lng = path.match(/locales\/(.*)\.json/)![1];
  modules[lng] = {
    translations: content as object,
  };
}

export async function initI18nextInstance() {
  // Configure instance
  i18n.use(LanguageDetector).use(initReactI18next);

  // Dispatch a global event when initialized so other bundles can react.
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
    lng: "en-US",
    debug: false,
    resources: modules,
    defaultNS: "translations",
    ns: ["translations"],
    fallbackLng: "en-US",
    detection: {
      order: ["navigator", "querystring", "htmlTag"],
      caches: [],
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
