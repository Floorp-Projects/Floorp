// SPDX-License-Identifier: MPL-2.0

import { parse } from "@std/toml";
import { createRootHMR } from "@nora/solid-xul";
import i18next from "i18next";
import { createEffect, createSignal } from "solid-js";
import { Resources } from "./default.d.ts";

const { I18nUtils } = ChromeUtils.importESModule(
  "resource://noraneko/modules/i18n/I18n-Utils.sys.mjs",
);

export let resources: Resources;

const _modules = import.meta.glob("./*/*.json", { eager: true });

const modules: Record<string, Record<string, object>> = {};
for (const [idx, m] of Object.entries(_modules)) {
  const [lng, ns] = idx.replaceAll("./", "").replaceAll(".json", "").split("/");
  if (!Object.hasOwn(modules, lng)) {
    modules[lng] = {};
  }
  modules[lng][ns] = (m as any).default as object;
}

const _meta = import.meta.glob("./*/_meta.toml", {
  eager: true,
  query: "?raw",
});
const fallbackLng: Record<string, string> = {};
for (const path in _meta) {
  fallbackLng[path.replaceAll("./", "").replaceAll("/_meta.toml", "")] = parse(
    _meta[path].default,
  )["fallback-language"] as string;
}

let i18nInitializedPromise: Promise<any> | null = null;
let pendingLocale: string | null = null;

export function initI18N(namespace: string[], defaultNamespace: string) {
  i18nInitializedPromise = i18next.init({
    lng: I18nUtils.getPrimaryBrowserLocaleMapped(),
    debug: false,
    resources: modules,
    defaultNS: defaultNamespace,
    ns: namespace,
    fallbackLng,
    interpolation: {
      escapeValue: false,
      defaultVariables: {
        productName: "Floorp",
      },
    },
  });

  i18nInitializedPromise.then(() => {
    if (pendingLocale) {
      i18next.changeLanguage(pendingLocale).then(async () => {
        if (!isSupportedLocale(pendingLocale!)) {
          await i18next.changeLanguage(fallbackLng["default"] || "en-US");
          setLang(i18next.language);
          return;
        }
        setLang(pendingLocale!);
        pendingLocale = null;
      });
    }
    // Register a translation provider with I18nUtils so chrome scripts and
    // other contexts can call into a shared accessor instead of racing on
    // module imports. This is a best-effort registration; callers should
    // gracefully handle absence of a provider.
    try {
      I18nUtils.registerTranslationProvider?.({
        t: i18next.t.bind(i18next),
        isInitializedPromise: i18nInitializedPromise!,
        changeLanguage: (lng: string) => {
          try {
            return i18next.changeLanguage(lng);
          } catch (e) {
            // Keep changeLanguage robust: don't allow throwing from provider
            // registration to break initialization. Log and re-throw so
            // callers can still handle the rejection if they expect it.
            try {
              console.error("Failed to change language", e);
            } catch {}
            throw e;
          }
        },
        getI18next: () => i18next,
      });
    } catch (e) {
      // Keep initialization robust: don't throw if registration fails.
      try {
        console.error(
          "Failed to register translation provider on I18nUtils",
          e,
        );
      } catch {}
    }
  });
}
const [lang, setLang] = createRootHMR(
  () => createSignal("ja-JP"),
  import.meta.hot,
);

I18nUtils.addLocaleChangeListener(async (newLocale: string) => {
  if (!i18nInitializedPromise) {
    pendingLocale = newLocale;
    return;
  }

  await i18nInitializedPromise;
  if (!isSupportedLocale(newLocale)) {
    await i18next.changeLanguage(fallbackLng["default"] || "en-US");
    setLang(i18next.language);
    return;
  }

  await i18next.changeLanguage(newLocale);
  setLang(newLocale);
});

/**
 * @param observer
 * @description For HMR, please run this function in `createRootHMR`
 * @example
 * ```ts
 * import { createRootHMR } from "@nora/solid-xul";
 *
 * createRootHMR(
 *   () => {
 *     addI18nObserver(observer);
 *   },
 *   import.meta.hot
 * );
 * ```
 */
export function addI18nObserver(observer: (locale: string) => void) {
  createEffect(() => {
    observer(lang());
  });
}

export function setLanguage(lang: string) {
  setLang(lang);
}

export function isSupportedLocale(locale: string) {
  return Object.keys(modules).includes(locale);
}
