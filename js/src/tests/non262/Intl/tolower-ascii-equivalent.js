// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Language tags are processed case-insensitive, but unconditionally calling
// the built-in String.prototype.toLowerCase() or toUpperCase() function
// before parsing a language tag can map non-ASCII characters into the ASCII
// range.
//
// Validate the BCP47 language tag parser handles this case (pun intended)
// correctly by passing language tags which contain U+212A (KELVIN SIGN) and
// U+0131 (LATIN SMALL LETTER DOTLESS I) to Intl.getCanonicalLocales().

// The lower-case form of "i-ha\u212A" is "i-hak".
assertEq("i-hak", "i-ha\u212A".toLowerCase());

// The upper-case form of "\u0131-hak" is "I-HAK".
assertEq("I-HAK", "\u0131-hak".toUpperCase());

// "i-hak" is a valid language tag.
assertEqArray(Intl.getCanonicalLocales("i-hak"), ["hak"]);

// But "i-ha\u212A" is not a valid language tag.
assertThrowsInstanceOf(() => Intl.getCanonicalLocales("i-ha\u212A"), RangeError);

// And "\u0131-hak" is also not a valid language tag.
assertThrowsInstanceOf(() => Intl.getCanonicalLocales("\u0131-hak"), RangeError);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
