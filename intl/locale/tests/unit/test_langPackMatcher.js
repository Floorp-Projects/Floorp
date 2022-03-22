/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { getAddonAndLocalAPIsMocker } = ChromeUtils.import(
  "resource://testing-common/LangPackMatcherTestUtils.jsm"
);
const { LangPackMatcher } = ChromeUtils.import(
  "resource://gre/modules/LangPackMatcher.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const sandbox = sinon.createSandbox();
const mockAddonAndLocaleAPIs = getAddonAndLocalAPIsMocker(this, sandbox);

add_task(function initSandbox() {
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(function test_appLocaleLanguageMismatch() {
  sandbox.restore();
  mockAddonAndLocaleAPIs({
    systemLocale: "es-ES",
    appLocale: "en-US",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "es-ES",
    systemLocale: { baseName: "es-ES", language: "es", region: "ES" },
    appLocaleRaw: "en-US",
    appLocale: { baseName: "en-US", language: "en", region: "US" },
    matchType: "language-mismatch",
    canLiveReload: true,
    displayNames: {
      systemLanguage: "European Spanish",
      appLanguage: "American English",
    },
  });
});

add_task(function test_appLocaleRegionMismatch() {
  sandbox.restore();
  mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "en-CA",
    appLocale: "en-US",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "en-CA",
    systemLocale: { baseName: "en-CA", language: "en", region: "CA" },
    appLocaleRaw: "en-US",
    appLocale: { baseName: "en-US", language: "en", region: "US" },
    matchType: "region-mismatch",
    canLiveReload: true,
    displayNames: {
      systemLanguage: "Canadian English",
      appLanguage: "American English",
    },
  });
});

add_task(function test_appLocaleScriptMismatch() {
  sandbox.restore();
  // Script mismatch:
  mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "zh-Hans-CN",
    appLocale: "zh-CN",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "zh-Hans-CN",
    systemLocale: { baseName: "zh-Hans-CN", language: "zh", region: "CN" },
    appLocaleRaw: "zh-CN",
    appLocale: { baseName: "zh-CN", language: "zh", region: "CN" },
    matchType: "match",
    canLiveReload: true,
    displayNames: {
      systemLanguage: "简体中文（中国）",
      appLanguage: "中文（中国）",
    },
  });
});

add_task(function test_appLocaleInvalidSystem() {
  sandbox.restore();
  // Script mismatch:
  mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "Not valid",
    appLocale: "en-US",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "Not valid",
    systemLocale: null,
    appLocaleRaw: "en-US",
    appLocale: { baseName: "en-US", language: "en", region: "US" },
    matchType: "unknown",
    canLiveReload: null,
    displayNames: { systemLanguage: null, appLanguage: "American English" },
  });
});

add_task(function test_bidiSwitchDisabled() {
  Services.prefs.setBoolPref(
    "intl.multilingual.liveReloadBidirectional",
    false
  );
  sandbox.restore();
  // Script mismatch:
  mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "ar-EG",
    appLocale: "en-US",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "ar-EG",
    systemLocale: { baseName: "ar-EG", language: "ar", region: "EG" },
    appLocaleRaw: "en-US",
    appLocale: { baseName: "en-US", language: "en", region: "US" },
    matchType: "language-mismatch",
    canLiveReload: false,
    displayNames: {
      systemLanguage: "Arabic (Egypt)",
      appLanguage: "American English",
    },
  });
  Services.prefs.clearUserPref("intl.multilingual.liveReloadBidirectional");
});

add_task(async function test_bidiSwitchEnabled() {
  Services.prefs.setBoolPref("intl.multilingual.liveReloadBidirectional", true);
  sandbox.restore();
  // Script mismatch:
  mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "ar-EG",
    appLocale: "en-US",
  });

  deepEqual(LangPackMatcher.getAppAndSystemLocaleInfo(), {
    systemLocaleRaw: "ar-EG",
    systemLocale: { baseName: "ar-EG", language: "ar", region: "EG" },
    appLocaleRaw: "en-US",
    appLocale: { baseName: "en-US", language: "en", region: "US" },
    matchType: "language-mismatch",
    canLiveReload: true,
    displayNames: {
      systemLanguage: "Arabic (Egypt)",
      appLanguage: "American English",
    },
  });

  Services.prefs.clearUserPref("intl.multilingual.liveReloadBidirectional");
});

function shuffle(array) {
  return array
    .map(value => ({ value, sort: Math.random() }))
    .sort((a, b) => a.sort - b.sort)
    .map(({ value }) => value);
}

add_task(async function test_negotiateLangPacks() {
  const negotiations = [
    {
      // Exact match found.
      systemLocale: "en-US",
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: "en-US",
    },
    {
      // Region-less match.
      systemLocale: "en-CA",
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: "en",
    },
    {
      // Fallback to a different region.
      systemLocale: "en-CA",
      availableLangPacks: ["en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: "en-US",
    },
    {
      // Match with a script. zh-Hans-CN is the locale used with simplified
      // Chinese scripts, while zh-CN uses the Latin script.
      systemLocale: "zh-Hans-CN",
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: "zh-Hans-CN",
    },
    {
      // No reasonable match could be found.
      systemLocale: "tlh", // Klingon
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: null,
    },
    {
      // Weird, but valid locale identifiers.
      systemLocale: "en-US-u-hc-h23-ca-islamic-civil-ss-true",
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: "en-US",
    },
    {
      // Invalid system locale
      systemLocale: "Not valid",
      availableLangPacks: ["en", "en-US", "zh", "zh-CN", "zh-Hans-CN"],
      expected: null,
    },
  ];

  for (const { systemLocale, availableLangPacks, expected } of negotiations) {
    sandbox.restore();
    const { resolveLangPacks } = mockAddonAndLocaleAPIs({
      sandbox,
      systemLocale,
    });

    const promise = LangPackMatcher.negotiateLangPackForLanguageMismatch();
    // Shuffle the order to ensure that this test doesn't require on ordering of the
    // langpack responses.
    resolveLangPacks(shuffle(availableLangPacks));
    const actual = (await promise)?.target_locale;
    equal(
      actual,
      expected,
      `Resolve the systemLocale "${systemLocale}" with available langpacks: ${JSON.stringify(
        availableLangPacks
      )}`
    );
  }
});

add_task(async function test_ensureLangPackInstalled() {
  sandbox.restore();
  const { resolveLangPacks, resolveInstaller } = mockAddonAndLocaleAPIs({
    sandbox,
    systemLocale: "es-ES",
    appLocale: "en-US",
  });

  const negotiatePromise = LangPackMatcher.negotiateLangPackForLanguageMismatch();
  resolveLangPacks(["es-ES"]);
  const langPack = await negotiatePromise;

  const installPromise1 = LangPackMatcher.ensureLangPackInstalled(langPack);
  const installPromise2 = LangPackMatcher.ensureLangPackInstalled(langPack);

  resolveInstaller(["fake langpack"]);

  info("Ensure both installers resolve when called twice in a row.");
  await installPromise1;
  await installPromise2;
  ok(true, "Both were called.");
});
