// SPDX-License-Identifier: MPL-2.0

export const defaultNS = "browser-chrome";
import { initI18N } from "./config.ts";
export function initI18NForBrowserChrome() {
  return initI18N(["browser-chrome"], "browser-chrome");
}

export { addI18nObserver, setLanguage, resources } from "./config.ts";
