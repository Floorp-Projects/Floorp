import { type PropsWithChildren, useEffect, useState } from "react";
import { initializeI18n } from "@/lib/i18n/init.ts";

export function I18nProvider({ children }: PropsWithChildren) {
  const [ready, setReady] = useState(false);

  useEffect(() => {
    let mounted = true;
    let cleanup: (() => void) | undefined;

    (async () => {
      try {
        const i18nInstance = await initializeI18n();

        document.documentElement.lang = i18nInstance.language;
        document.title = i18nInstance.t("title.default");

        const handleLanguageChanged = (lng: string) => {
          document.documentElement.lang = lng;
          document.title = i18nInstance.t("title.default");
        };
        i18nInstance.on("languageChanged", handleLanguageChanged);
        cleanup = () => i18nInstance.off("languageChanged", handleLanguageChanged);
      } catch (e) {
        console.error("[I18nProvider] initialization failed:", e);
      }
      if (mounted) setReady(true);
    })();
    return () => {
      mounted = false;
      cleanup?.();
    };
  }, []);

  if (!ready) return <div />;

  return <>{children}</>;
}
