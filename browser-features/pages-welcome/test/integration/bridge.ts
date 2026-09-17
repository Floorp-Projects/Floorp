import {
  calls,
  expose,
  installBridge,
  prefs,
} from "../../../../libs/ui/test/page-fixture.ts";
export const state = {
  engine: "google",
  locale: "en-US",
  installed: ["en-US"],
  fail: "",
};
export function setup() {
  installBridge();
  expose("NRDismissWelcomePage", () => {
    calls.push({ method: "NRDismissWelcomePage", args: [] });
  });
  prefs.set("layout.css.prefers-color-scheme.content-override", 2);
  const engines = ["google", "duckduckgo", "bing"].map((identifier) => ({
    identifier,
    id: identifier,
    name: identifier,
    isAppProvided: true,
    iconURL: null,
  }));
  expose(
    "NRGetSearchEngines",
    (callback: (data: string) => void) => callback(JSON.stringify(engines)),
  );
  expose(
    "NRGetDefaultEngine",
    (callback: (data: string) => void) =>
      callback(
        JSON.stringify(
          engines.find((item) => item.identifier === state.engine),
        ),
      ),
  );
  expose(
    "NRSetDefaultEngine",
    (id: string, callback: (data: string) => void) => {
      const success = state.fail !== "engine";
      if (success) state.engine = id;
      calls.push({ method: "NRSetDefaultEngine", args: [id] });
      callback(JSON.stringify({ success, engineId: id }));
    },
  );
  expose(
    "NRGetLocaleInfo",
    (callback: (data: string) => void) =>
      callback(JSON.stringify({
        availableLocales: ["en-US", "en-GB"].map((target_locale) => ({
          target_locale,
          hash: "test",
          url: "https://example.invalid/langpack.xpi",
        })),
        installedLocales: state.installed,
        localeInfo: {
          appLocaleRaw: state.locale,
          systemLocaleRaw: "en-US",
          systemLocale: { baseName: "en-US", language: "en", region: "US" },
          displayNames: { systemLanguage: "English (US)" },
        },
      })),
  );
  expose(
    "NRGetNativeNames",
    (codes: string[], callback: (data: string) => void) =>
      callback(JSON.stringify(codes)),
  );
  expose(
    "NRInstallLangPack",
    (pack: { target_locale: string }, callback: (data: string) => void) => {
      const success = state.fail !== "install";
      calls.push({ method: "NRInstallLangPack", args: [pack.target_locale] });
      if (success) state.installed.push(pack.target_locale);
      callback(JSON.stringify({ success, locale: pack.target_locale }));
    },
  );
  expose(
    "NRSetAppLocale",
    (locale: string, callback: (data: string) => void) => {
      const success = state.fail !== "locale";
      if (success) state.locale = locale;
      calls.push({ method: "NRSetAppLocale", args: [locale] });
      callback(JSON.stringify({ success, locale }));
    },
  );
  expose("NRSetDefaultBrowser", (callback: (data: string) => void) => {
    calls.push({ method: "NRSetDefaultBrowser", args: [] });
    callback(JSON.stringify({ success: state.fail !== "default" }));
  });
}
