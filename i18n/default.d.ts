import * as nsDefault from "./en-US/default.json" with { type: "json" };
import * as nsBrowserChrome from "./en-US/browser-chrome.json" with { type: "json" };

export type Resources = {
  default: typeof nsDefault.default,
  "browser-chrome": typeof nsBrowserChrome.default,
};