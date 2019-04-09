// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Unicode locale extension sequences don't allow keys with a digit as their
// second character.
assertThrowsInstanceOf(() => Intl.getCanonicalLocales("en-u-c0"), RangeError);
assertThrowsInstanceOf(() => Intl.getCanonicalLocales("en-u-00"), RangeError);

// The first character is allowed to be a digit.
assertEq(Intl.getCanonicalLocales("en-u-0c")[0], "en-u-0c");

if (typeof reportCompare === "function")
    reportCompare(true, true);
