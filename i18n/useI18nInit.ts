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
    console.info("[initI18n] start: checking i18n initialization");
    // Ensure i18next has finished its initialization and the internal
    // store is available before calling changeLanguage. In some
    // production bundles i18n.init may be async and initI18n can run
    // earlier, causing `this.store` to be undefined inside i18next.
    if (!i18n.isInitialized) {
      console.info(
        "[initI18n] i18n not initialized yet; waiting for 'initialized' event",
      );
      await new Promise<void>((resolve) => {
        const onInit = () => {
          try {
            i18n.off("initialized", onInit);
          } catch {
            /* ignore */
          }
          console.info("[initI18n] received i18n 'initialized' event");
          resolve();
        };
        i18n.on("initialized", onInit);
      });
    } else {
      console.info("[initI18n] i18n.isInitialized === true");
    }

    const g = globalThis as unknown as {
      NRI18n?: { getPrimaryBrowserLocaleMapped?: () => Promise<string> };
    };
    console.info(
      "[initI18n] requesting primary browser locale from actor (if available)",
    );
    const locale = await g.NRI18n?.getPrimaryBrowserLocaleMapped?.();
    console.info("[initI18n] got locale from actor:", locale);
    console.info(
      "[initI18n] calling i18n.changeLanguage ->",
      locale || "en-US",
    );
    await i18n.changeLanguage(locale || "en-US");
    console.info("[initI18n] i18n.changeLanguage completed");
  } catch (e) {
    console.error(
      "[initI18n] Failed to get browser locale or apply language:",
      e,
    );
    try {
      if (!i18n.isInitialized) {
        console.info(
          "[initI18n] (fallback) waiting for i18n 'initialized' event before applying fallback",
        );
        await new Promise<void>((resolve) => {
          const onInit = () => {
            try {
              i18n.off("initialized", onInit);
            } catch {
              /* ignore */
            }
            console.info(
              "[initI18n] (fallback) received i18n 'initialized' event",
            );
            resolve();
          };
          i18n.on("initialized", onInit);
        });
      }
      console.info("[initI18n] (fallback) calling changeLanguage('en-US')");
      await i18n.changeLanguage("en-US");
      console.info("[initI18n] (fallback) changeLanguage completed");
    } catch (e2) {
      console.error("[initI18n] Failed to change language fallback:", e2);
    }
  }
}
