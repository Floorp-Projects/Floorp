// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// Ensure alias and parent locales are correctly picked up when calling into ICU.

// "zh-HK" is an alias to "zh-Hant-HK", so display names should default to
// traditional instead of simplified characters.
{
  const zh_Hant = new Intl.DisplayNames("zh-Hant", {type: "region"});
  const zh_Hans = new Intl.DisplayNames("zh-Hans", {type: "region"});
  const zh_HK = new Intl.DisplayNames("zh-HK", {type: "region"});

  // We assume traditional and simplified have different outputs.
  assertEq(zh_Hant.of("US") === zh_Hans.of("US"), false);

  // "zh-HK" should use traditional characters.
  assertEq(zh_HK.of("US"), zh_Hant.of("US"));
}

// The parent locale of "en-AU" is "en-001" and not "en" (because "en" actually means "en-US").
{
  const en = new Intl.DisplayNames("en", {type: "language"});
  const en_001 = new Intl.DisplayNames("en-001", {type: "language"});
  const en_AU = new Intl.DisplayNames("en-AU", {type: "language"});

  // We assume "en" and "en-001" have different outputs.
  assertEq(en.of("nds-NL") === en_001.of("nds-NL"), false);

  // "en-AU" should have the same output as "en-001".
  assertEq(en_AU.of("nds-NL"), en_001.of("nds-NL"));
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
