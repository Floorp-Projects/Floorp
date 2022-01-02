// Ensure we properly quote strings which can contain the NUL character before
// returning them to the user to avoid cutting off any trailing characters.

function assertStringIncludes(actual, expected) {
    assertEq(actual.includes(expected), true, `"${actual}" includes "${expected}"`);
}

function assertErrorMessageIncludes(fn, str) {
    try {
        fn();
    } catch (e) {
        assertStringIncludes(e.message, str);
        return;
    }
    assertEq(true, false, "missing exception");
}

assertErrorMessageIncludes(() => "foo\0bar" in "asdf\0qwertz", "bar");
assertErrorMessageIncludes(() => "foo\0bar" in "asdf\0qwertz", "qwertz");

if (this.Intl) {
    assertErrorMessageIncludes(() => Intl.getCanonicalLocales("de\0Latn"), "Latn");

    assertErrorMessageIncludes(() => Intl.Collator.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.Collator("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.Collator("en", {usage: "sort\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.Collator("en", {caseFirst: "upper\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.Collator("en", {sensitivity: "base\0cookie"}), "cookie");

    assertErrorMessageIncludes(() => Intl.DateTimeFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {hourCycle: "h24\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {weekday: "narrow\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {era: "narrow\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {year: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {month: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {day: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {hour: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {minute: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {second: "2-digit\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {formatMatcher: "basic\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.DateTimeFormat("en", {timeZone: "UTC\0cookie"}), "cookie");

    assertErrorMessageIncludes(() => Intl.NumberFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.NumberFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.NumberFormat("en", {style: "decimal\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.NumberFormat("en", {currency: "USD\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.NumberFormat("en", {currencyDisplay: "code\0cookie"}), "cookie");

    assertErrorMessageIncludes(() => Intl.PluralRules.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.PluralRules("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.PluralRules("en", {type: "cardinal\0cookie"}), "cookie");

    assertErrorMessageIncludes(() => Intl.RelativeTimeFormat.supportedLocalesOf([], {localeMatcher:"lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.RelativeTimeFormat("en", {localeMatcher: "lookup\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.RelativeTimeFormat("en", {style: "long\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.RelativeTimeFormat("en", {numeric: "auto\0cookie"}), "cookie");
    assertErrorMessageIncludes(() => new Intl.RelativeTimeFormat().format(1, "day\0cookie"), "cookie");

    assertErrorMessageIncludes(() => new Intl.Locale("de\0keks"), "keks");
    assertErrorMessageIncludes(() => new Intl.Locale("de", {language: "it\0biscotto"}), "biscotto");
    assertErrorMessageIncludes(() => new Intl.Locale("th", {script: "Thai\0คุกกี้"}), "\\u0E04\\u0E38\\u0E01\\u0E01\\u0E35\\u0E49");
    assertErrorMessageIncludes(() => new Intl.Locale("en", {region: "GB\0biscuit"}), "biscuit");

    assertErrorMessageIncludes(() => new Intl.Locale("und", {calendar: "gregory\0F1"}), "F1");
    assertErrorMessageIncludes(() => new Intl.Locale("und", {collation: "phonebk\0F2"}), "F2");
    assertErrorMessageIncludes(() => new Intl.Locale("und", {hourCycle: "h24\0F3"}), "F3");
    assertErrorMessageIncludes(() => new Intl.Locale("und", {caseFirst: "false\0F4"}), "F4");
    assertErrorMessageIncludes(() => new Intl.Locale("und", {numberingSystem: "arab\0F5"}), "F5");
}

// Shell and helper functions.

assertErrorMessageIncludes(() => assertEq(true, false, "foo\0bar"), "bar");

if (this.disassemble) {
    assertStringIncludes(disassemble(Function("var re = /foo\0bar/")), "bar");
}

if (this.getBacktrace) {
    const k = "asdf\0asdf";
    const o = {[k](a) { "use strict"; return getBacktrace({locals: true, args: true}); }};
    const r = o[k].call("foo\0bar", "baz\0faz");
    assertStringIncludes(r, "bar");
    assertStringIncludes(r, "faz");
}

// js/src/tests/browser.js provides its own |options| function, make sure we don't test that one.
if (this.options && typeof document === "undefined") {
    assertErrorMessageIncludes(() => options("foo\0bar"), "bar");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
