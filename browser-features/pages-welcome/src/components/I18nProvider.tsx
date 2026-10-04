import { useEffect, useState } from "react";
import { I18nextProvider } from "react-i18next";
import i18n, { initializeI18n } from "@/lib/i18n/i18n";

export function I18nProvider({ children }: { children: React.ReactNode }) {
  const [initialized, setInitialized] = useState(false);

  useEffect(() => {
    let active = true;
    const updateDocumentLanguage = () => {
      document.documentElement.lang = i18n.language;
      document.documentElement.dir = i18n.dir(i18n.language);
    };
    i18n.on("languageChanged", updateDocumentLanguage);
    const init = async () => {
      await initializeI18n();
      if (active) {
        updateDocumentLanguage();
        setInitialized(true);
      }
    };
    void init();
    return () => {
      active = false;
      i18n.off("languageChanged", updateDocumentLanguage);
    };
  }, []);

  if (!initialized) {
    return null; // Or a loading spinner
  }

  return <I18nextProvider i18n={i18n}>{children}</I18nextProvider>;
}
