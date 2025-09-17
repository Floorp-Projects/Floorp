// SPDX-License-Identifier: MPL-2.0

import { parse } from "@std/toml";
import { createRootHMR } from "@nora/solid-xul";
import i18next from "i18next";
import { createEffect, createSignal } from "solid-js";
import { Resources } from "./default.d.ts";

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


export function initI18N(namespace: string[], defaultNamespace: string) {
  i18next.init({
    lng: "en-US",
    debug: true,
    resources: modules,
    defaultNS: defaultNamespace,
    ns: namespace,
    fallbackLng,
  });
}
const [lang, setLang] = createRootHMR(
  () => createSignal("ja-JP"),
  import.meta.hot,
);

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
