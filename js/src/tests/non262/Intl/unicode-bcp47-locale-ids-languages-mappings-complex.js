// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// CLDR contains language mappings where in addition to the language subtag also
// the script or region subtag is modified, unless they're already present.

// <languageAlias type="sh" replacement="sr_Latn" reason="legacy"/>
assertEq(Intl.getCanonicalLocales("sh")[0], "sr-Latn");
assertEq(Intl.getCanonicalLocales("sh-RS")[0], "sr-Latn-RS");
assertEq(Intl.getCanonicalLocales("sh-Cyrl")[0], "sr-Cyrl");

// <languageAlias type="cnr" replacement="sr_ME" reason="legacy"/>
assertEq(Intl.getCanonicalLocales("cnr")[0], "sr-ME");
assertEq(Intl.getCanonicalLocales("cnr-Latn")[0], "sr-Latn-ME");
assertEq(Intl.getCanonicalLocales("cnr-RS")[0], "sr-RS");

// Aliases where more than just a language subtag are present are ignored.
// <languageAlias type="sr_RS" replacement="sr_Cyrl_RS" reason="legacy"/>
assertEq(Intl.getCanonicalLocales("sr-RS")[0], "sr-RS");

if (typeof reportCompare === "function")
    reportCompare(true, true);
