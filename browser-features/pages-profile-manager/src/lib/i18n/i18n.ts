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
  i18n.use(LanguageDetector).use(initReactI18next);
  // Determine the initial language depending on environment.
  // - In development use the window actor API (NRI18n) provided by actors/NRI18nChild.sys.mts
  // - In production (real chrome environment) use I18n-Utils via ChromeUtils.importESModule
  let initialLng = "en-US";
  try {
    if (import.meta.env.DEV) {
      // development: actor exposes NRI18n on window
      const g = globalThis as unknown as {
        NRI18n?: { getPrimaryBrowserLocaleMapped?: () => Promise<string> };
      };
      const maybe = await g.NRI18n?.getPrimaryBrowserLocaleMapped?.();
      if (maybe) initialLng = maybe;
    } else {
      try {
        // production: use chrome module I18n-Utils
        // Note: use .sys.mjs resource path to match other imports in repo
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore - ChromeUtils is available in chrome privileged contexts
        const { I18nUtils } = ChromeUtils.importESModule(
          "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
        );
        if (I18nUtils?.getPrimaryBrowserLocaleMapped) {
          initialLng = I18nUtils.getPrimaryBrowserLocaleMapped();
        }
      } catch {
        // keep fallback
      }
    }
  } catch {
    // keep default
  }

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
    lng: initialLng,
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
