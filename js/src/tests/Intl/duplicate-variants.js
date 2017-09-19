// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// RFC 5646 section 2.1
// variant       = 5*8alphanum         ; registered variants
//               / (DIGIT 3alphanum)

// Duplicate variants are forbidden.
assertEqArray(Intl.getCanonicalLocales("de-1996"), ["de-1996"]);
assertThrowsInstanceOf(() => Intl.getCanonicalLocales("de-1996-1996"), RangeError);

// Multiple different variants are allowed.
assertEqArray(Intl.getCanonicalLocales("sl-rozaj-biske-1994"), ["sl-rozaj-biske-1994"]);

// Variants can have the same prefix.
assertEqArray(Intl.getCanonicalLocales("zh-Latn-pinyin-pinyin2"), ["zh-Latn-pinyin-pinyin2"]);

// Values in extension sequences are not counted as variants.
assertEqArray(Intl.getCanonicalLocales("en-u-kf-false-kn-false"), ["en-u-kf-false-kn-false"]);

// Also test duplicates in Unicode extension keywords and attributes.
// From https://tools.ietf.org/html/rfc6067#section-2.1
//
//    An 'attribute' is a subtag with a length of three to eight
//    characters following the singleton and preceding any 'keyword'
//    sequences.  No attributes were defined at the time of this
//    document's publication.
//
//    A 'keyword' is a sequence of subtags consisting of a 'key' subtag,
//    followed by zero or more 'type' subtags (so a 'key' might appear
//    alone and not be accompanied by a 'type' subtag).  A 'key' MUST
//    NOT appear more than once in a language tag's extension string.
//
//    ...
//
//    Only the first occurrence of an attribute or key conveys meaning in a
//    language tag.  When interpreting tags containing the Unicode locale
//    extension, duplicate attributes or keywords are ignored in the
//    following way: ignore any attribute that has already appeared in the
//    tag and ignore any keyword whose key has already occurred in the tag.
assertEqArray(Intl.getCanonicalLocales("en-u-kn-false-kn-false"), ["en-u-kn-false-kn-false"]);
assertEqArray(Intl.getCanonicalLocales("en-u-attr1-attr2-attr2"), ["en-u-attr1-attr2-attr2"]);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
