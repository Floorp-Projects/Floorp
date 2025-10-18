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

console.info("[i18n] initializing i18n (pages-settings)");

i18n.use(LanguageDetector).use(initReactI18next);

// Attach listener before init to capture the event
i18n.on("initialized", (opts) => {
  console.info("[i18n] 'initialized' event fired (pages-settings)", opts);
});

i18n.init({
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

export default i18n;
