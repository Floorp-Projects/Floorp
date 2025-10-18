import { useEffect } from "react";
import { initI18nextInstance } from "@/lib/i18n/i18n.ts";

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
        await initI18nextInstance();
        const g = globalThis as unknown as {
          NRI18n?: { getPrimaryBrowserLocaleMapped?: () => Promise<string> };
        };
        const locale = await g.NRI18n?.getPrimaryBrowserLocaleMapped?.();
        if (locale) {
          const i18n = (await import("@/lib/i18n/i18n.ts")).default;
          await i18n.changeLanguage(locale);
        }
      } catch (e) {
        console.error("Failed to get browser locale or initialize i18n:", e);
        try {
          const i18n = (await import("@/lib/i18n/i18n.ts")).default;
          await i18n.changeLanguage("en-US");
        } catch (e2) {
          console.error("Failed to apply fallback language:", e2);
        }
      }
    })();
  }, []);
}
