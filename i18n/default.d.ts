// SPDX-License-Identifier: MPL-2.0

import type * as nsDefault from "./en-US/default.json" with { type: "json" };
import type * as nsBrowserChrome from "./en-US/browser-chrome.json" with { type: "json" };

export type Resources = {
  default: typeof nsDefault.default,
  "browser-chrome": typeof nsBrowserChrome.default,
};