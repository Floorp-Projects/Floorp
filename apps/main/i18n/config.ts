import i18next from "i18next";

import { en_US } from "./en-US/en-US";
import { ja_JP } from "./ja-JP/ja_JP";

import { createEffect, createSignal } from "solid-js";

export const defaultNS = "undo";

export const resources = {
  "en-US": en_US,
  "ja-JP": ja_JP,
};
export function initI18N() {
  i18next.init({
    lng: "en-US",
    debug: true,
    resources,
    defaultNS,
    ns: ["undo"],
    fallbackLng: ["en-US", "dev"],
  });
}
const [lang, setLang] = createSignal("ja-JP");
/**
 *
 * @param observer
 * @description For HMR, please run this function in `createRootHMR`
 * @example
 * ```ts
 * import { createRootHMR } from "@nora/solid-xul";
 *
 * createRootHMR(
 *   () => {
 *     addI18NObserver(observer);
 *   },
 *   import.meta.hot
 * );
 * ```
 */
export function addI8NObserver(observer: (locale: string) => void) {
  createEffect(() => {
    observer(lang());
  });
}
