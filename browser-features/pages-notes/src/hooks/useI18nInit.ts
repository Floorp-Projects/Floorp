import { useEffect } from "react";
import i18n from "i18next";

declare global {
  // eslint-disable-next-line @typescript-eslint/naming-convention
  interface GlobalThis {
    NRI18n?: {
      getPrimaryBrowserLocaleMapped?: () => Promise<string>;
    };
  }
}

export default function useI18nInit() {
  useEffect(() => {
    (async () => {
      try {
        const locale =
          await globalThis.NRI18n?.getPrimaryBrowserLocaleMapped?.();
        console.log("Initializing language to:", locale);
        i18n.changeLanguage(locale || "en-US");
      } catch (e) {
        console.error("Failed to get browser locale:", e);
        i18n.changeLanguage("en-US");
      }
    })();
  }, []);
}
