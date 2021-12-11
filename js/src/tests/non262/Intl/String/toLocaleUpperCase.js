// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test language dependent special casing with different language tags.
for (let locale of ["lt", "LT", "lt-LT", "lt-u-co-phonebk", "lt-x-lietuva"]) {
    assertEq("i\u0307".toLocaleUpperCase(locale), "I");
    assertEq("i\u0307".toLocaleUpperCase([locale]), "I");

    // Additional language tags are ignored.
    assertEq("i\u0307".toLocaleUpperCase([locale, "und"]), "I");
    assertEq("i\u0307".toLocaleUpperCase(["und", locale]), "I\u0307");
}

// Ensure "lti" (Leti) isn't misrecognized as "lt", even though both share the
// same prefix.
assertEq("i\u0307".toLocaleUpperCase("lti"), "I\u0307");
assertEq("i\u0307".toLocaleUpperCase(["lti"]), "I\u0307");

// Language tag is always verified.
for (let locale of ["no_locale", "lt-invalid_ext", ["no_locale"], ["en", "no_locale"]]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleUpperCase(locale), RangeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "a".toLocaleUpperCase(locale), RangeError);
}

// No locale argument, undefined as locale, and empty array or array-like all
// return the same result. Testing with "a/A" because it has only simple case
// mappings.
assertEq("a".toLocaleUpperCase(), "A");
assertEq("a".toLocaleUpperCase(undefined), "A");
assertEq("a".toLocaleUpperCase([]), "A");
assertEq("a".toLocaleUpperCase({}), "A");
assertEq("a".toLocaleUpperCase({length: 0}), "A");
assertEq("a".toLocaleUpperCase({length: -1}), "A");

// Test with incorrect locale type.
for (let locale of [null, 0, Math.PI, NaN, Infinity, true, false, Symbol()]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleUpperCase([locale]), TypeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "a".toLocaleUpperCase([locale]), TypeError);
}

// Primitives are converted with ToObject and then queried for .length property.
for (let locale of [null]) {
    // Empty input string.
    assertThrowsInstanceOf(() => "".toLocaleUpperCase([locale]), TypeError);

    // Non-empty input string.
    assertThrowsInstanceOf(() => "a".toLocaleUpperCase([locale]), TypeError);
}
// ToLength(ToObject(<primitive>)) returns 0.
for (let locale of [0, Math.PI, NaN, Infinity, true, false, Symbol()]) {
    // Empty input string.
    assertEq("".toLocaleUpperCase(locale), "");

    // Non-empty input string.
    assertEq("a".toLocaleUpperCase(locale), "A");
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
