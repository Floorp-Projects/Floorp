import i18n from "i18next";

declare global {
  // eslint-disable-next-line @typescript-eslint/naming-convention
  interface GlobalThis {
    NRI18n?: {
      getPrimaryBrowserLocaleMapped?: () => Promise<string>;
    };
  }
}

export async function initI18n() {
  try {
    const g = globalThis as unknown as {
      NRI18n?: { getPrimaryBrowserLocaleMapped?: () => Promise<string> };
    };
    const locale = await g.NRI18n?.getPrimaryBrowserLocaleMapped?.();
    i18n.changeLanguage(locale || "en-US");
  } catch (e) {
    console.error("Failed to get browser locale:", e);
    i18n.changeLanguage("en-US");
  }
}
