// SPDX-License-Identifier: MPL-2.0

import type { defaultNS } from "#i18n/config-browser-chrome.ts";

declare module "i18next" {
  interface CustomTypeOptions {
    defaultNS: typeof defaultNS;
    // NOTE: `resources` omitted to allow dynamic key usage in t() calls.
    // Re-add `resources: typeof resources` when all call sites use static keys.
  }
}
