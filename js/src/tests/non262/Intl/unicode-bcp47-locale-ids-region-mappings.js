// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// For the most part the mappings from IANA are a subset of the CLDR mappings.
// So there are mappings which are consistent across both databases.
assertEq(Intl.getCanonicalLocales("de-DD")[0], "de-DE");

// CLDR contains additional mappings:
//   <territoryAlias type="QU" replacement="EU" reason="deprecated"/>
//   <territoryAlias type="UK" replacement="GB" reason="deprecated"/>
assertEq(Intl.getCanonicalLocales("und-QU")[0], "und-EU");
assertEq(Intl.getCanonicalLocales("en-UK")[0], "en-GB");

// CLDR additional maps ISO 3166-1 numeric to ISO 3166-1 alpha-2 codes:
//   <territoryAlias type="280" replacement="DE" reason="deprecated"/>
//   <territoryAlias type="278" replacement="DE" reason="overlong"/>
//   <territoryAlias type="276" replacement="DE" reason="overlong"/>
assertEq(Intl.getCanonicalLocales("de-280")[0], "de-DE");
assertEq(Intl.getCanonicalLocales("de-278")[0], "de-DE");
assertEq(Intl.getCanonicalLocales("de-276")[0], "de-DE");

if (typeof reportCompare === "function")
    reportCompare(true, true);
