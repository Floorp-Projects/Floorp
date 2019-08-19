// |reftest| skip-if(!this.hasOwnProperty("Intl"))

function isConstructor(c) {
    return typeof c === "function" && c.hasOwnProperty("prototype");
}

const IntlServices = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(isConstructor);

// Examples for all possible Unicode extension keys (with the exception of deprecated "vt").
const unicodeExtensions = [
    // calendar
    "ca-gregory",
    "fw-mon",
    "hc-h23",

    // collation
    "co-phonebk",
    "ka-noignore",
    "kb-false",
    "kc-false",
    "kf-false",
    "kh-false",
    "kk-false",
    "kn-false",
    "kr-space",
    "ks-level1",
    "kv-space",

    // currency
    "cf-standard",
    "cu-eur",

    // measure
    "ms-metric",

    // number
    "nu-latn",

    // segmentation
    "lb-strict",
    "lw-normal",
    "ss-none",

    // timezone
    "tz-atvie",

    // variant
    "em-default",
    "rg-atzzzz",
    "sd-atat1",
    "va-posix",
];

function reverse(a, b) {
    if (a < b) {
        return 1;
    }
    if (a > b) {
        return -1;
    }
    return 0;
}

function findUnicodeExtensionKeys(locale) {
    // Find the Unicode extension key subtag.
    var extension = locale.match(/.*-u-(.*)/);
    if (extension === null) {
        return [];
    }

    // Replace all types in the Unicode extension keywords.
    return extension[1].replace(/-\w{3,}/g, "").split("-");
}

// Test all Intl service constructors and ensure the Unicode extension keys in
// the resolved locale are sorted alphabetically.

for (let IntlService of IntlServices) {
    // sort() modifies the input array, so create a copy.
    let ext = unicodeExtensions.slice(0);

    let locale, keys;

    // Input keys unsorted.
    locale = new IntlService(`de-u-${ext.join("-")}`).resolvedOptions().locale;
    keys = findUnicodeExtensionKeys(locale);
    assertEqArray(keys, keys.slice(0).sort());

    // Input keys sorted alphabetically.
    locale = new IntlService(`de-u-${ext.sort().join("-")}`).resolvedOptions().locale;
    keys = findUnicodeExtensionKeys(locale);
    assertEqArray(keys, keys.slice(0).sort());

    // Input keys sorted alphabetically in reverse order.
    locale = new IntlService(`de-u-${ext.sort(reverse).join("-")}`).resolvedOptions().locale;
    keys = findUnicodeExtensionKeys(locale);
    assertEqArray(keys, keys.slice(0).sort());
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
