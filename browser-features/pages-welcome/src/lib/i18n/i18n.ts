import i18n from "i18next";
import { initReactI18next } from "react-i18next";

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

export const initializeI18n = async () => {
  let osLocale = "en-US";
  try {
    // @ts-ignore
    osLocale = await window.NRI18n.getOperatingSystemLocale();
  } catch (e) {
    console.error("Failed to get OS locale from NRI18n, falling back to en-US", e);
  }

  await i18n
    .use(initReactI18next)
    .init({
      lng: osLocale,
      debug: true,
      resources: modules,
      defaultNS: "translations",
      ns: ["translations"],
      fallbackLng: "en-US",
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
};

export default i18n;
