// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// ECMA-402 includes mapping of legacy variants, as long as they're also present
// in <variantAlias> in CLDR's supplementalMetadata.xml
// <https://www.unicode.org/reports/tr35/#Legacy_Variants>

assertEq(Intl.getCanonicalLocales("sv-AALAND")[0], "sv-AX");
assertEq(Intl.getCanonicalLocales("no-BOKMAL")[0], "nb-bokmal");
assertEq(Intl.getCanonicalLocales("no-NYNORSK")[0], "nb-nynorsk");
assertEq(Intl.getCanonicalLocales("en-POSIX")[0], "en-posix");
assertEq(Intl.getCanonicalLocales("el-POLYTONI")[0], "el-polyton");
assertEq(Intl.getCanonicalLocales("aa-SAAHO")[0], "aa-saaho");

if (typeof reportCompare === "function")
    reportCompare(true, true);
