import { initI18nextInstance } from "@/lib/i18n/i18n.ts";

export async function initializeI18n() {
  const i18n = await initI18nextInstance();

  try {
    const g = globalThis as unknown as {
      NRI18n?: { getPrimaryBrowserLocaleMapped?: () => Promise<string> };
    };
    const locale = await g.NRI18n?.getPrimaryBrowserLocaleMapped?.();
    if (locale) {
      await i18n.changeLanguage(locale);
    }
  } catch (e) {
    console.error(
      "[i18n:init] Failed to get/apply locale, falling back to en-US",
      e,
    );
    try {
      await i18n.changeLanguage("en-US");
    } catch {
      /* ignore */
    }
  }

  return i18n;
}
