// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test language dependent special casing with different language tags.
for (let locale of ["tr", "TR", "tr-TR", "tr-u-co-search", "tr-x-turkish"]) {
    assertEq("\u0130".toLocaleLowerCase(locale), "i");
    assertEq("\u0130".toLocaleLowerCase([locale]), "i");

    // Additional language tags are ignored.
    assertEq("\u0130".toLocaleLowerCase([locale, "und"]), "i");
    assertEq("\u0130".toLocaleLowerCase(["und", locale]), "\u0069\u0307");
}

// Ensure "trl" (Traveller Scottish) isn't misrecognized as "tr", even though
// both share the same prefix.
assertEq("\u0130".toLocaleLowerCase("trl"), "\u0069\u0307");
assertEq("\u0130".toLocaleLowerCase(["trl"]), "\u0069\u0307");

// Language tag is always verified.
for (let locale of ["no_locale", "tr-invalid_ext", ["no_locale"], ["en", "no_locale"]]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleLowerCase(locale), RangeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "x".toLocaleLowerCase(locale), RangeError);
}

// The language tag fast-path for String.prototype.toLocaleLowerCase doesn't
// trip up on three element private-use only language tags.
assertEq("A".toLocaleLowerCase("x-x"), "a");
assertEq("A".toLocaleLowerCase("x-0"), "a");

// No locale argument, undefined as locale, and empty array or array-like all
// return the same result. Testing with "a/A" because it has only simple case
// mappings.
assertEq("A".toLocaleLowerCase(), "a");
assertEq("A".toLocaleLowerCase(undefined), "a");
assertEq("A".toLocaleLowerCase([]), "a");
assertEq("A".toLocaleLowerCase({}), "a");
assertEq("A".toLocaleLowerCase({length: 0}), "a");
assertEq("A".toLocaleLowerCase({length: -1}), "a");

// Test with incorrect locale type.
for (let locale of [null, 0, Math.PI, NaN, Infinity, true, false, Symbol()]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleLowerCase([locale]), TypeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "A".toLocaleLowerCase([locale]), TypeError);
}

// Primitives are converted with ToObject and then queried for .length property.
for (let locale of [null]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleLowerCase([locale]), TypeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "A".toLocaleLowerCase([locale]), TypeError);
}
// ToLength(ToObject(<primitive>)) returns 0.
for (let locale of [0, Math.PI, NaN, Infinity, true, false, Symbol()]) {
    // Empty input string.
    assertEq("".toLocaleLowerCase(locale), "");

    // Non-empty input string.
    assertEq("A".toLocaleLowerCase(locale), "a");
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
