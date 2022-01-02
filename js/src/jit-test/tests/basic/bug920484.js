load(libdir + "asserts.js");

// Add an invalid "localeMatcher" option to Object.prototype, the exact value does not matter,
// any value except "best fit" or "lookup" is okay.
Object.prototype.localeMatcher = "invalid matcher option";

// The Intl API may not be available in the testing environment.
if (this.hasOwnProperty("Intl")) {
    // Intl constructors still work perfectly fine. The default options object doesn't inherit
    // from Object.prototype and the invalid "localeMatcher" value from Object.prototype isn't
    // consulted.
    new Intl.Collator().compare("a", "b");
    new Intl.NumberFormat().format(10);
    new Intl.DateTimeFormat().format(new Date);

    // If an explicit "localeMatcher" option is given, the default from Object.prototype is ignored.
    new Intl.Collator(void 0, {localeMatcher: "lookup"}).compare("a", "b");
    new Intl.NumberFormat(void 0, {localeMatcher: "lookup"}).format(10);
    new Intl.DateTimeFormat(void 0, {localeMatcher: "lookup"}).format(new Date);

    delete Object.prototype.localeMatcher;

    // After removing the default option from Object.prototype, everything still works as expected.
    new Intl.Collator().compare("a", "b");
    new Intl.NumberFormat().format(10);
    new Intl.DateTimeFormat().format(new Date);
}
