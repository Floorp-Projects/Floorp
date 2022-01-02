// |reftest| skip-if(!this.hasOwnProperty("Intl")||!xulRuntime.shell)

// js/src/tests/lib/tests.py sets the default locale to "en-US" for shell tests.
// Ensure it's correctly set in the runtime and for the Intl service constructors.
const defaultLocale = "en-US";

assertEq(getDefaultLocale(), defaultLocale);

assertEq(new Intl.Collator().resolvedOptions().locale, defaultLocale);
assertEq(new Intl.DateTimeFormat().resolvedOptions().locale, defaultLocale);
assertEq(new Intl.NumberFormat().resolvedOptions().locale, defaultLocale);
assertEq(new Intl.PluralRules().resolvedOptions().locale, defaultLocale);
assertEq(new Intl.RelativeTimeFormat().resolvedOptions().locale, defaultLocale);

if (typeof reportCompare === "function")
    reportCompare(true, true);
