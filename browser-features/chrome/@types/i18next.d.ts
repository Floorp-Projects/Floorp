import type { defaultNS, resources } from "#i18n/config-browser-chrome.ts";

declare module "i18next" {
  interface CustomTypeOptions {
    defaultNS: typeof defaultNS;
    resources: typeof resources;
  }
}
