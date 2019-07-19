// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// ECMA-402 doesn't include mapping of legacy variants.
// <https://www.unicode.org/reports/tr35/#Legacy_Variants>

assertEq(Intl.getCanonicalLocales("sv-AALAND")[0], "sv-aaland");
assertEq(Intl.getCanonicalLocales("no-BOKMAL")[0], "nb-bokmal");
assertEq(Intl.getCanonicalLocales("no-NYNORSK")[0], "nb-nynorsk");
assertEq(Intl.getCanonicalLocales("en-POSIX")[0], "en-posix");
assertEq(Intl.getCanonicalLocales("el-POLYTONI")[0], "el-polytoni");
assertEq(Intl.getCanonicalLocales("aa-SAAHO")[0], "aa-saaho");

if (typeof reportCompare === "function")
    reportCompare(true, true);
