import { useEffect, useState } from "react";
import { I18nextProvider } from "react-i18next";
import i18n, { initializeI18n } from "@/lib/i18n/i18n";

export function I18nProvider({ children }: { children: React.ReactNode }) {
  const [initialized, setInitialized] = useState(false);

  useEffect(() => {
    const init = async () => {
      await initializeI18n();
      setInitialized(true);
    };
    init();
  }, []);

  if (!initialized) {
    return null; // Or a loading spinner
  }

  return <I18nextProvider i18n={i18n}>{children}</I18nextProvider>;
}
