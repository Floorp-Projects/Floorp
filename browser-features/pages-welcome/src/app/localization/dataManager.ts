import type { LangPack, LocaleData } from "@/app/localization/type.ts";

export async function getLocaleData(): Promise<LocaleData> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRGetLocaleInfo((data: string) => {
      const localeInfo = JSON.parse(data);
      resolve(localeInfo);
    });
  });
}

export async function installLangPack(langPack: LangPack): Promise<{
  locale: string;
  success: boolean;
}> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRInstallLangPack(langPack, (data: string) => {
      resolve(JSON.parse(data));
    });
  });
}

export async function setAppLocale(locale: string): Promise<{
  locale: string;
  success: boolean;
}> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRSetAppLocale(locale, (data: string) => {
      resolve(JSON.parse(data));
    });
  });
}

export async function getNativeNames(langCodes: string[]): Promise<string[]> {
  return await new Promise((resolve) => {
    // deno-lint-ignore no-window
    window.NRGetNativeNames(langCodes, (data: string) => {
      resolve(JSON.parse(data));
    });
  });
}
