// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// Intl.ListFormat.supportedLocalesOf returns an empty array for unsupported locales.
assertEq(Intl.ListFormat.supportedLocalesOf("art-lobjan").length, 0);

// And a non-empty array for supported locales.
assertEq(Intl.ListFormat.supportedLocalesOf("en").length, 1);
assertEq(Intl.ListFormat.supportedLocalesOf("en")[0], "en");

// If the locale is supported per |Intl.ListFormat.supportedLocalesOf|, the resolved locale
// should reflect this.
for (let locale of Intl.ListFormat.supportedLocalesOf(["en", "de", "th", "ar"])) {
    let lf = new Intl.ListFormat(locale);
    assertEq(lf.resolvedOptions().locale, locale);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
