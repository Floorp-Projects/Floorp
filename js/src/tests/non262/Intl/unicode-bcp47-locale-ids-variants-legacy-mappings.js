// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// ECMA-402 includes mapping of legacy variants, as long as they're also present
// in <variantAlias> in CLDR's supplementalMetadata.xml
// <https://www.unicode.org/reports/tr35/#Legacy_Variants>

assertEq(Intl.getCanonicalLocales("sv-AALAND")[0], "sv-AX");
assertEq(Intl.getCanonicalLocales("no-BOKMAL")[0], "nb");
assertEq(Intl.getCanonicalLocales("no-NYNORSK")[0], "nn");
assertEq(Intl.getCanonicalLocales("en-POSIX")[0], "en-posix");
assertEq(Intl.getCanonicalLocales("el-POLYTONI")[0], "el-polyton");
assertEq(Intl.getCanonicalLocales("aa-SAAHO")[0], "ssy");

// Additional cases from CLDR, version 38.

assertEq(Intl.getCanonicalLocales("aar-saaho")[0], "ssy");
assertEq(Intl.getCanonicalLocales("arm-arevmda")[0], "hyw");
assertEq(Intl.getCanonicalLocales("hy-arevmda")[0], "hyw");
assertEq(Intl.getCanonicalLocales("hye-arevmda")[0], "hyw");

for (let language of ["chi", "cmn", "zh", "zho"]) {
    assertEq(Intl.getCanonicalLocales(language)[0], "zh");

    let mapping = {
        "guoyu-hakka": "hak",
        "guoyu-xiang": "hsn",
        "guoyu": "zh",
        "hakka": "hak",
        "xiang": "hsn",
    };
    for (let [variant, canonical] of Object.entries(mapping)) {
        assertEq(Intl.getCanonicalLocales(`${language}-${variant}`)[0], canonical);
    }
}

// Most legacy variant subtags are simply removed in contexts they don't apply.
for (let variant of ["arevela", "arevmda", "bokmal", "hakka", "lojban", "nynorsk", "saaho", "xiang"]) {
    assertEq(Intl.getCanonicalLocales(`en-${variant}`)[0], "en");
}

// Except these three, whose replacement is always applied.
assertEq(Intl.getCanonicalLocales("en-aaland")[0], "en-AX");
assertEq(Intl.getCanonicalLocales("en-heploc")[0], "en-alalc97");
assertEq(Intl.getCanonicalLocales("en-polytoni")[0], "en-polyton");

if (typeof reportCompare === "function")
    reportCompare(true, true);
