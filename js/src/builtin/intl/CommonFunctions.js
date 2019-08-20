/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Portions Copyright Norbert Lindenberg 2011-2012. */

#ifdef DEBUG
#define assertIsValidAndCanonicalLanguageTag(locale, desc) \
    do { \
        let localeObj = parseLanguageTag(locale); \
        assert(localeObj !== null, \
               `${desc} is a structurally valid language tag`); \
        CanonicalizeLanguageTagObject(localeObj); \
        assert(StringFromLanguageTagObject(localeObj) === locale, \
               `${desc} is a canonicalized language tag`); \
    } while (false)
#else
#define assertIsValidAndCanonicalLanguageTag(locale, desc) ; // Elided assertion.
#endif

/**
 * Returns the start index of a "Unicode locale extension sequence", which the
 * specification defines as: "any substring of a language tag that starts with
 * a separator '-' and the singleton 'u' and includes the maximum sequence of
 * following non-singleton subtags and their preceding '-' separators."
 *
 * Alternatively, this may be defined as: the components of a language tag that
 * match the `unicode_locale_extensions` production in UTS 35.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.1.
 */
function startOfUnicodeExtensions(locale) {
    assert(typeof locale === "string", "locale is a string");

    // Search for "-u-" marking the start of a Unicode extension sequence.
    var start = callFunction(std_String_indexOf, locale, "-u-");
    if (start < 0)
        return -1;

    // And search for "-x-" marking the start of any privateuse component to
    // handle the case when "-u-" was only found within a privateuse subtag.
    var privateExt = callFunction(std_String_indexOf, locale, "-x-");
    if (privateExt >= 0 && privateExt < start)
        return -1;

    return start;
}

/**
 * Returns the end index of a Unicode locale extension sequence.
 */
function endOfUnicodeExtensions(locale, start) {
    assert(typeof locale === "string", "locale is a string");
    assert(0 <= start && start < locale.length, "start is an index into locale");
    assert(Substring(locale, start, 3) === "-u-", "start points to Unicode extension sequence");

    #define HYPHEN 0x2D
    assert(std_String_fromCharCode(HYPHEN) === "-",
           "code unit constant should match the expected character");

    // Search for the start of the next singleton or privateuse subtag.
    //
    // Begin searching after the smallest possible Unicode locale extension
    // sequence, namely |"-u-" 2alphanum|. End searching once the remaining
    // characters can't fit the smallest possible singleton or privateuse
    // subtag, namely |"-x-" alphanum|. Note the reduced end-limit means
    // indexing inside the loop is always in-range.
    for (var i = start + 5, end = locale.length - 4; i <= end; i++) {
        if (callFunction(std_String_charCodeAt, locale, i) !== HYPHEN)
            continue;
        if (callFunction(std_String_charCodeAt, locale, i + 2) === HYPHEN)
            return i;

        // Skip over (i + 1) and (i + 2) because we've just verified they
        // aren't "-", so the next possible delimiter can only be at (i + 3).
        i += 2;
    }

    #undef HYPHEN

    // If no singleton or privateuse subtag was found, the Unicode extension
    // sequence extends until the end of the string.
    return locale.length;
}

/**
 * Removes Unicode locale extension sequences from the given language tag.
 */
function removeUnicodeExtensions(locale) {
    assertIsValidAndCanonicalLanguageTag(locale, "locale with possible Unicode extension");

    var start = startOfUnicodeExtensions(locale);
    if (start < 0)
        return locale;

    var end = endOfUnicodeExtensions(locale, start);

    var left = Substring(locale, 0, start);
    var right = Substring(locale, end, locale.length - end);
    var combined = left + right;

    assertIsValidAndCanonicalLanguageTag(combined, "the recombined locale");
    assert(startOfUnicodeExtensions(combined) < 0,
           "recombination failed to remove all Unicode locale extension sequences");

    return combined;
}

/**
 * Returns Unicode locale extension sequences from the given language tag.
 */
function getUnicodeExtensions(locale) {
    assertIsValidAndCanonicalLanguageTag(locale, "locale with Unicode extension");

    var start = startOfUnicodeExtensions(locale);
    assert(start >= 0, "start of Unicode extension sequence not found");
    var end = endOfUnicodeExtensions(locale, start);

    return Substring(locale, start, end - start);
}

// The three possible token type bits. Expressed as #defines to avoid
// extra named lookups in the interpreter/jits.
#define NONE  0b00
#define ALPHA 0b01
#define DIGIT 0b10

// Constants for code units used below.
#define HYPHEN  0x2D
#define DIGIT_ZERO 0x30
#define DIGIT_NINE 0x39
#define UPPER_A 0x41
#define UPPER_Z 0x5A
#define LOWER_A 0x61
#define LOWER_T 0x74
#define LOWER_U 0x75
#define LOWER_X 0x78
#define LOWER_Z 0x7A

// The requirement to use callFunction() for method calls makes the parser
// harder to read. Use macros for the rescue.

// Reads the next token.
#define NEXT_TOKEN_OR_RETURN_NULL(ts)       \
    if (!callFunction(ts.nextToken, ts))    \
        return null;

#ifdef DEBUG
#define NEXT_TOKEN_OR_ASSERT(ts)            \
    if (!callFunction(ts.nextToken, ts))    \
        assert(false, "unexpected invalid subtag");
#else
#define NEXT_TOKEN_OR_ASSERT(ts)            \
    callFunction(ts.nextToken, ts);
#endif

// Assigns the current subtag part transformed to lower-case to the target.
#define SUBTAG_VAR_OR_RETURN_NULL(ts, target)                                   \
    {                                                                           \
        target = Substring(ts.localeLowercase, ts.tokenStart, ts.tokenLength);  \
        NEXT_TOKEN_OR_RETURN_NULL(ts);                                          \
    }

// Assigns the current subtag part transformed to lower-case to the target.
#define SUBTAG_VAR_OR_ASSERT(ts, target)                                        \
    {                                                                           \
        target = Substring(ts.localeLowercase, ts.tokenStart, ts.tokenLength);  \
        NEXT_TOKEN_OR_ASSERT(ts)                                                \
    }

/**
 * Tokenizer for Unicode BCP 47 locale identifiers.
 */
function BCP47TokenStream(locale) {
    this.locale = locale;

    // Locale identifiers are compared and processed case-insensitively, so
    // technically it's not necessary to adjust case. But for easier processing,
    // and because the canonical form for most subtags is lower case, we start
    // with lower case for all.
    //
    // Note that the tokenizer function keeps using the original input string
    // to properly detect non-ASCII characters. The lower-case string can't be
    // used to detect those characters, because some non-ASCII characters
    // lower-case map into ASCII characters, e.g. U+212A (KELVIN SIGN) lower-
    // case maps to U+006B (LATIN SMALL LETTER K).
    this.localeLowercase = callFunction(std_String_toLowerCase, locale);

    // Current parse index in |locale|.
    this.index = 0;

    // The current token type, its start index, and its length.
    this.token = NONE;
    this.tokenStart = 0;
    this.tokenLength = 0;

    assert(std_String_fromCharCode(HYPHEN) === "-" &&
           std_String_fromCharCode(DIGIT_ZERO) === "0" &&
           std_String_fromCharCode(DIGIT_NINE) === "9" &&
           std_String_fromCharCode(UPPER_A) === "A" &&
           std_String_fromCharCode(UPPER_Z) === "Z" &&
           std_String_fromCharCode(LOWER_A) === "a" &&
           std_String_fromCharCode(LOWER_T) === "t" &&
           std_String_fromCharCode(LOWER_U) === "u" &&
           std_String_fromCharCode(LOWER_X) === "x" &&
           std_String_fromCharCode(LOWER_Z) === "z",
           "code unit constants should match the expected characters");
}

MakeConstructible(BCP47TokenStream, {
    __proto__: null,

    // Reads the next token, returns |false| if an illegal character was found,
    // otherwise returns |true|.
    //
    // eslint-disable-next-line object-shorthand
    nextToken: function() {
        var type = NONE;
        var {index, locale} = this;
        for (var i = index; i < locale.length; i++) {
            // UTS 35, section 3.1.
            // alpha = [A-Z a-z] ;
            // digit = [0-9] ;
            var c = callFunction(std_String_charCodeAt, locale, i);
            if ((UPPER_A <= c && c <= UPPER_Z) || (LOWER_A <= c && c <= LOWER_Z))
                type |= ALPHA;
            else if (DIGIT_ZERO <= c && c <= DIGIT_NINE)
                type |= DIGIT;
            else if (c === HYPHEN && i > index && i + 1 < locale.length)
                break;
            else
                return false;
        }

        this.token = type;
        this.tokenStart = index;
        this.tokenLength = i - index;
        this.index = i + 1;
        return true;
    },

    // Returns true if the character at the requested index within the current
    // token is a digit.
    //
    // eslint-disable-next-line object-shorthand
    isDigitAt: function(index) {
        assert(0 <= index && index < this.tokenLength,
               "must be an index into the current token");
        var c = callFunction(std_String_charCodeAt, this.localeLowercase, this.tokenStart + index);
        assert(!(c <= DIGIT_NINE) || c >= DIGIT_ZERO,
               "token-start-code-unit <= '9' implies token-start-code-unit is in '0'..'9' " +
               "and because all digits are sorted before any letters");
        return c <= DIGIT_NINE;
    },

    // Returns the code unit of the first character at the current token
    // position. Always returns the lower-case form of an alphabetical
    // character.
    //
    // eslint-disable-next-line object-shorthand
    singletonKey: function() {
        assert(this.tokenLength === 1, "token is not a singleton");
        var c = callFunction(std_String_charCodeAt, this.localeLowercase, this.tokenStart);
        assert((DIGIT_ZERO <= c && c <= DIGIT_NINE) || (LOWER_A <= c && c <= LOWER_Z),
               "unexpected code unit");
        return c;
    },

    // eslint-disable-next-line object-shorthand
    singletonValue: function() {
        var singletonStart = this.tokenStart;
        var min = callFunction(this.singletonKey, this) === LOWER_X ? 1 : 2;

        NEXT_TOKEN_OR_RETURN_NULL(this);

        // At least one non-singleton subtag must be present.
        if (!(min <= this.tokenLength && this.tokenLength <= 8))
            return null;
        do {
            NEXT_TOKEN_OR_RETURN_NULL(this);
        } while (min <= this.tokenLength && this.tokenLength <= 8);

        return callFunction(this.singletonValueAt, this, singletonStart);
    },

    // eslint-disable-next-line object-shorthand
    singletonValueAt: function(start) {
        // Singletons must be followed by a non-singleton subtag, "en-a-b" is not allowed.
        var length = this.tokenStart - 1 - start;
        if (length <= 2)
            return null;
        return Substring(this.localeLowercase, start, length);
    }
});

/* eslint-disable complexity */
/**
 * Parser for Unicode BCP 47 locale identifiers.
 *
 * Returns null if |locale| can't be parsed as a `unicode_locale_id`. If the
 * input is a grandfathered language tag, it is directly canonicalized to its
 * modern form. The returned object has the following structure:
 *
 *   {
 *     language: `unicode_language_subtag`,
 *     script: `unicode_script_subtag` / undefined,
 *     region: `unicode_region_subtag` / undefined,
 *     variants: array of `unicode_variant_subtag`,
 *     extensions: array of `extensions`,
 *     privateuse: `pu_extensions` / undefined,
 *   }
 *
 * All locale identifier subtags are returned in their normalized case:
 *
 *   var langtag = parseLanguageTag("en-latn-us");
 *   assertEq("en", langtag.language);
 *   assertEq("Latn", langtag.script);
 *   assertEq("US", langtag.region);
 *
 * Spec: https://unicode.org/reports/tr35/#Unicode_Language_and_Locale_Identifiers
 */
function parseLanguageTag(locale) {
    assert(typeof locale === "string", "locale is a string");

    // unicode_locale_id = unicode_language_id
    //                     extensions*
    //                     pu_extensions? ;
    var ts = new BCP47TokenStream(locale);
    NEXT_TOKEN_OR_RETURN_NULL(ts);

    var language, script, region, privateuse;
    var variants = [];
    var extensions = [];

    // unicode_language_id = unicode_language_subtag
    //                       (sep unicode_script_subtag)?
    //                       (sep unicode_region_subtag)?
    //                       (sep unicode_variant_subtag)* ;
    //
    // sep                 = "-"
    //
    // Note: Unicode CLDR locale identifier backward compatibility extensions
    //       removed from `unicode_language_id`.

    // unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
    if (ts.token !== ALPHA || ts.tokenLength === 1 || ts.tokenLength === 4 || ts.tokenLength > 8) {
        // Four character language subtags are not allowed in Unicode BCP 47
        // locale identifiers. Also see the comparison to Unicode CLDR locale
        // identifiers in <https://unicode.org/reports/tr35/#BCP_47_Conformance>.
        return null;
    }
    assert((2 <= ts.tokenLength && ts.tokenLength <= 3) ||
           (5 <= ts.tokenLength && ts.tokenLength <= 8),
           "language subtags have 2-3 or 5-8 letters");

    SUBTAG_VAR_OR_RETURN_NULL(ts, language);

    // unicode_script_subtag = alpha{4} ;
    if (ts.tokenLength === 4 && ts.token === ALPHA) {
        SUBTAG_VAR_OR_RETURN_NULL(ts, script);

        // The first character of a script code needs to be capitalized.
        // "hans" -> "Hans"
        script = callFunction(std_String_toUpperCase, script[0]) +
                 Substring(script, 1, script.length - 1);
    }

    // unicode_region_subtag = (alpha{2} | digit{3}) ;
    if ((ts.tokenLength === 2 && ts.token === ALPHA) ||
        (ts.tokenLength === 3 && ts.token === DIGIT))
    {
        SUBTAG_VAR_OR_RETURN_NULL(ts, region);

        // Region codes need to be in upper-case. "bu" -> "BU"
        region = callFunction(std_String_toUpperCase, region);
    }

    // unicode_variant_subtag = (alphanum{5,8}
    //                        | digit alphanum{3}) ;
    //
    // alphanum               = [0-9 A-Z a-z] ;
    while ((5 <= ts.tokenLength && ts.tokenLength <= 8) ||
           (ts.tokenLength === 4 && callFunction(ts.isDigitAt, ts, 0)))
    {
        // Locale identifiers are case insensitive (UTS 35, section 3.2).
        // All seen variants are compared ignoring case differences by
        // using the lower-case form. This allows to properly detect and
        // reject variant repetitions with differing case, e.g.
        // "en-variant-Variant".
        var variant;
        SUBTAG_VAR_OR_RETURN_NULL(ts, variant);

        // Reject the Locale identifier if a duplicate variant was found.
        //
        // This linear-time verification step means the whole variant
        // subtag checking is potentially quadratic, but we're okay doing
        // that because language tags are unlikely to be deliberately
        // pathological.
        if (callFunction(ArrayIndexOf, variants, variant) !== -1)
            return null;
        _DefineDataProperty(variants, variants.length, variant);
    }

    // extensions = unicode_locale_extensions
    //            | transformed_extensions
    //            | other_extensions ;
    //
    // unicode_locale_extensions = sep [uU]
    //                             ((sep keyword)+
    //                             |(sep attribute)+ (sep keyword)*) ;
    //
    // transformed_extensions = sep [tT]
    //                          ((sep tlang (sep tfield)*)
    //                          |(sep tfield)+) ;
    //
    // other_extensions = [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
    //
    // keyword = key (sep type)? ;
    //
    // key = alphanum alpha ;
    //
    // type = alphanum{3,8} (sep alphanum{3,8})* ;
    //
    // attribute = alphanum{3,8} ;
    //
    // tlang = unicode_language_subtag
    //         (sep unicode_script_subtag)?
    //         (sep unicode_region_subtag)?
    //         (sep unicode_variant_subtag)* ;
    //
    // tfield = tkey tvalue;
    //
    // tkey = alpha digit ;
    //
    // tvalue = (sep alphanum{3,8})+ ;
    var seenSingletons = [];
    while (ts.tokenLength === 1) {
        var singleton = callFunction(ts.singletonKey, ts);
        if (singleton === LOWER_X)
            break;

        // Locale identifiers are case insensitive (UTS 35, section 3.2).
        // Ensure |singletonKey()| does not return the code unit of an
        // upper-case character, so we can properly detect and reject
        // singletons with different case, e.g. "en-u-foo-U-foo".
        assert(!(UPPER_A <= singleton && singleton <= UPPER_Z),
               "unexpected upper-case code unit");

        // Reject the input if a duplicate singleton was found.
        //
        // Similar to the variant validation step this check is O(n**2),
        // but given that there are only 35 possible singletons the
        // quadratic runtime is negligible.
        if (callFunction(ArrayIndexOf, seenSingletons, singleton) !== -1)
            return null;
        _DefineDataProperty(seenSingletons, seenSingletons.length, singleton);

        var extension;
        if (singleton === LOWER_U) {
            var extensionStart = ts.tokenStart;
            NEXT_TOKEN_OR_RETURN_NULL(ts);

            while (2 <= ts.tokenLength && ts.tokenLength <= 8) {
                // `key` doesn't allow a digit as its second character.
                if (ts.tokenLength === 2 && callFunction(ts.isDigitAt, ts, 1))
                    return null;
                NEXT_TOKEN_OR_RETURN_NULL(ts);
            }
            extension = callFunction(ts.singletonValueAt, ts, extensionStart);
        } else if (singleton === LOWER_T) {
            var extensionStart = ts.tokenStart;
            NEXT_TOKEN_OR_RETURN_NULL(ts);

            // `tfield` starts with `tkey`, which in turn is `alpha digit`, so
            // an alpha-only token must be a `tlang`.
            if (ts.token === ALPHA) {
                // `unicode_language_subtag`
                if (ts.tokenLength === 1 || ts.tokenLength === 4 || ts.tokenLength > 8)
                    return null;
                NEXT_TOKEN_OR_RETURN_NULL(ts);

                // `unicode_script_subtag` (optional)
                if (ts.tokenLength === 4 && ts.token === ALPHA) {
                    NEXT_TOKEN_OR_RETURN_NULL(ts);
                }

                // `unicode_region_subtag` (optional)
                if ((ts.tokenLength === 2 && ts.token === ALPHA) ||
                    (ts.tokenLength === 3 && ts.token === DIGIT))
                {
                    NEXT_TOKEN_OR_RETURN_NULL(ts);
                }

                // `unicode_variant_subtag` (optional)
                while ((5 <= ts.tokenLength && ts.tokenLength <= 8) ||
                       (ts.tokenLength === 4 && callFunction(ts.isDigitAt, ts, 0)))
                {
                    NEXT_TOKEN_OR_RETURN_NULL(ts);
                }
            }

            // Trailing `tfield` subtags.
            while (ts.tokenLength === 2) {
                // `tkey` is `alpha digit`.
                if (callFunction(ts.isDigitAt, ts, 0) ||
                    !callFunction(ts.isDigitAt, ts, 1))
                {
                    return null;
                }
                NEXT_TOKEN_OR_RETURN_NULL(ts);

                // `tfield` requires at least one `tvalue`.
                if (!(3 <= ts.tokenLength && ts.tokenLength <= 8))
                    return null;
                do {
                    NEXT_TOKEN_OR_RETURN_NULL(ts);
                } while (3 <= ts.tokenLength && ts.tokenLength <= 8);
            }
            extension = callFunction(ts.singletonValueAt, ts, extensionStart);
        } else {
            extension = callFunction(ts.singletonValue, ts);
        }
        if (!extension)
            return null;

        _DefineDataProperty(extensions, extensions.length, extension);
    }

    // Trailing pu_extensions component of the unicode_locale_id production.
    //
    // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
    if (ts.tokenLength === 1 && callFunction(ts.singletonKey, ts) === LOWER_X) {
        privateuse = callFunction(ts.singletonValue, ts);
        if (!privateuse)
            return null;
    }

    // Reject the input if it couldn't be parsed completely.
    if (ts.token !== NONE)
        return null;

    var tagObj = {
        language,
        script,
        region,
        variants,
        extensions,
        privateuse,
    };

    // Handle grandfathered tags right away, so we don't need to have extra
    // paths for grandfathered tags later on.
    //
    // grandfathered = "art-lojban"     ; non-redundant tags registered
    //               / "cel-gaulish"    ; during the RFC 3066 era
    //               / "zh-guoyu"       ; these tags match the 'langtag'
    //               / "zh-hakka"       ; production, but their subtags
    //               / "zh-xiang"       ; are not extended language
    //                                  ; or variant subtags: their meaning
    //                                  ; is defined by their registration
    //                                  ; and all of these are deprecated
    //                                  ; in favor of a more modern
    //                                  ; subtag or sequence of subtags
    if (hasOwn(ts.localeLowercase, grandfatheredMappings))
        updateGrandfatheredMappings(tagObj);

    // Return if the complete input was successfully parsed.
    return tagObj;
}

/**
 * Returns the input normalized to lower case if it matches the
 * 'unicode_language_subtag' production. Otherwise returns null.
 */
function parseStandaloneLanguage(language) {
    // unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
    var length = language.length;
    if (length < 2 || length === 4 || length > 8 || !IsASCIIAlphaString(language)) {
        // Four character language subtags are not allowed in Unicode BCP 47
        // locale identifiers. Also see the comparison to Unicode CLDR locale
        // identifiers in <https://unicode.org/reports/tr35/#BCP_47_Conformance>.
        return null;
    }

    return callFunction(std_String_toLowerCase, language);
}

/**
 * Returns the input normalized to title case if it matches the
 * 'unicode_script_subtag' production. Otherwise returns null.
 */
function parseStandaloneScript(script) {
    // unicode_script_subtag = alpha{4} ;
    if (script.length !== 4 || !IsASCIIAlphaString(script)) {
        return null;
    }

    // The first character of a script code needs to be capitalized.
    // "hans" -> "Hans"
    return callFunction(std_String_toUpperCase, script[0]) +
           callFunction(std_String_toLowerCase, Substring(script, 1, script.length - 1));
}

/**
 * Returns the input normalized to upper case if it matches the
 * 'unicode_region_subtag' production. Otherwise returns null.
 */
function parseStandaloneRegion(region) {
    // unicode_region_subtag = (alpha{2} | digit{3}) ;
    var length = region.length;
    if ((length !== 2 || !IsASCIIAlphaString(region)) &&
        (length !== 3 || !IsASCIIDigitString(region)))
    {
        return null;
    }

    // Region codes need to be in upper-case. "bu" -> "BU"
    return callFunction(std_String_toUpperCase, region);
}

/**
 * Returns the input normalized to lower case if it can be parsed as a
 * '(3*8alphanum) *("-" (3*8alphanum))' subtag sequence. Otherwise returns
 * null.
 */
function parseStandaloneUnicodeExtensionType(type) {
    // Reuse the BCP 47 parser for Unicode extension types.
    var ts = new BCP47TokenStream(type);
    NEXT_TOKEN_OR_RETURN_NULL(ts);

    // Unicode extension 'type' subtags must match the following ABNF.
    //
    // type     = (3*8alphanum) *("-" (3*8alphanum))
    // alphanum = (ALPHA / DIGIT)       ; letters and numbers
    // ALPHA    = %x41-5A / %x61-7A     ; A-Z / a-z
    // DIGIT    = %x30-39               ; 0-9
    do {
        if (ts.tokenLength < 3 || ts.tokenLength > 8)
            return null;

        NEXT_TOKEN_OR_RETURN_NULL(ts);
    } while (ts.token !== NONE);

    return ts.localeLowercase;
}

/**
 * Return the locale and fields components of the given valid Transform
 * extension subtag.
 */
function TransformExtensionComponents(extension) {
    assert(typeof extension === "string", "extension is a String value");
    assert(callFunction(std_String_startsWith, extension, "t-"),
           "extension starts with 't-'");

    var ts = new BCP47TokenStream(Substring(extension, 2, extension.length - 2));
    NEXT_TOKEN_OR_ASSERT(ts);

    // `tfield` starts with `tkey`, which in turn is `alpha digit`, so
    // an alpha-only token must be a `tlang`.
    var localeObj;
    if (ts.token === ALPHA) {
        // `unicode_language_subtag`
        assert((2 <= ts.tokenLength && ts.tokenLength <= 3) ||
                (5 <= ts.tokenLength && ts.tokenLength <= 8),
                "language subtags have 2-3 or 5-8 letters");

        var language;
        SUBTAG_VAR_OR_ASSERT(ts, language);

        // unicode_script_subtag = alpha{4} ;
        var script;
        if (ts.tokenLength === 4 && ts.token === ALPHA) {
            SUBTAG_VAR_OR_ASSERT(ts, script);

            // The first character of a script code needs to be capitalized.
            // "hans" -> "Hans"
            script = callFunction(std_String_toUpperCase, script[0]) +
                     Substring(script, 1, script.length - 1);
        }

        // unicode_region_subtag = (alpha{2} | digit{3}) ;
        var region;
        if ((ts.tokenLength === 2 && ts.token === ALPHA) ||
            (ts.tokenLength === 3 && ts.token === DIGIT))
        {
            SUBTAG_VAR_OR_ASSERT(ts, region);

            // Region codes need to be in upper-case. "bu" -> "BU"
            region = callFunction(std_String_toUpperCase, region);
        }

        // unicode_variant_subtag = (alphanum{5,8}
        //                        | digit alphanum{3}) ;
        //
        // alphanum               = [0-9 A-Z a-z] ;
        var variants = [];
        while ((5 <= ts.tokenLength && ts.tokenLength <= 8) ||
               (ts.tokenLength === 4 && callFunction(ts.isDigitAt, ts, 0)))
        {
            var variant;
            SUBTAG_VAR_OR_ASSERT(ts, variant);

            _DefineDataProperty(variants, variants.length, variant);
        }

        localeObj = {
            language,
            script,
            region,
            variants,
            extensions: [],
            privateuse: undefined,
        };
    }

    // Trailing `tfield` subtags. (Any other trailing subtags are an error,
    // because we're guaranteed to only see a valid tranform extension here.)
    var fields = [];
    while (ts.tokenLength === 2) {
        // `tkey` is `alpha digit`.
        assert(!callFunction(ts.isDigitAt, ts, 0) && callFunction(ts.isDigitAt, ts, 1),
               "unexpected invalid tkey subtag");

        var key;
        SUBTAG_VAR_OR_ASSERT(ts, key);

        // `tfield` requires at least one `tvalue`.
        assert(3 <= ts.tokenLength && ts.tokenLength <= 8,
               "unexpected invalid tvalue subtag");

        var value;
        SUBTAG_VAR_OR_ASSERT(ts, value);

        while (3 <= ts.tokenLength && ts.tokenLength <= 8) {
            var part;
            SUBTAG_VAR_OR_ASSERT(ts, part);
            value += "-" + part;
        }

        _DefineDataProperty(fields, fields.length, {key, value});
    }

    assert(ts.token === NONE,
           "unexpected trailing characters in promised-to-be-valid transform extension");

    return {locale: localeObj, fields};
}
/* eslint-enable complexity */

#undef NONE
#undef ALPHA
#undef DIGIT

#undef HYPHEN
#undef DIGIT_ZERO
#undef DIGIT_NINE
#undef UPPER_A
#undef UPPER_Z
#undef LOWER_A
#undef LOWER_T
#undef LOWER_U
#undef LOWER_X
#undef LOWER_Z

#undef SUBTAG_VAR_OR_ASSERT
#undef SUBTAG_VAR_OR_RETURN_NULL
#undef NEXT_TOKEN_OR_ASSERT
#undef NEXT_TOKEN_OR_RETURN_NULL

/**
 * Verifies that the given string is a well-formed BCP 47 language tag
 * with no duplicate variant or singleton subtags.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.2.
 */
function IsStructurallyValidLanguageTag(locale) {
    return parseLanguageTag(locale) !== null;
}

/**
 * Canonicalizes the given structurally valid Unicode BCP 47 locale identifier,
 * including regularized case of subtags. For example, the language tag
 * Zh-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE, where
 *
 *     Zh             ; 2*3ALPHA
 *     -haNS          ; ["-" script]
 *     -bu            ; ["-" region]
 *     -variant2      ; *("-" variant)
 *     -Variant1
 *     -u-ca-chinese  ; *("-" extension)
 *     -t-Zh-laTN
 *     -x-PRIVATE     ; ["-" privateuse]
 *
 * becomes zh-Hans-MM-variant1-variant2-t-zh-latn-u-ca-chinese-x-private
 *
 * UTS 35 specifies two different canonicalization algorithms. There's one to
 * canonicalize BCP 47 language tags and other one to canonicalize Unicode
 * locale identifiers. The latter one wasn't present when ECMA-402 was changed
 * to use Unicode BCP 47 locale identifiers instead of BCP 47 language tags, so
 * ECMA-402 currently only uses the former to canonicalize Unicode BCP 47 locale
 * identifiers.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.3.
 * Spec: https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers
 * Spec: https://unicode.org/reports/tr35/#BCP_47_Language_Tag_Conversion
 */
function CanonicalizeLanguageTagObject(localeObj) {
    assert(IsObject(localeObj), "CanonicalizeLanguageTagObject");

    // Per UTS 35, 3.3.1, the very first step is to canonicalize the syntax by
    // normalizing the case and ordering all subtags. The canonical syntax form
    // itself is specified in UTS 35, 3.2.1.

    // The parser already normalized the case for all subtags.

#ifdef DEBUG
    function IsLowerCase(s) {
        return s === callFunction(std_String_toLowerCase, s);
    }
    function IsUpperCase(s) {
        return s === callFunction(std_String_toUpperCase, s);
    }
    function IsTitleCase(s) {
        assert(s.length > 0, "unexpected empy string");
        var r = callFunction(std_String_toUpperCase, s[0]) +
                callFunction(std_String_toLowerCase, Substring(s, 1, s.length - 1));
        return s === r;
    }
#endif

    // 1. Any script subtag is in title case.
    assert(localeObj.script === undefined || IsTitleCase(localeObj.script),
           "If present, script subtag is in title case");

    // 2. Any region subtag is in uppercase.
    assert(localeObj.region === undefined || IsUpperCase(localeObj.region),
           "If present, region subtag is in upper case");

    // 3. All other subtags are in lowercase.
    assert(IsLowerCase(localeObj.language),
           "language subtag is in lower case");
    assert(callFunction(ArrayEvery, localeObj.variants, IsLowerCase),
           "variant subtags are in lower case");
    assert(callFunction(ArrayEvery, localeObj.extensions, IsLowerCase),
           "extension subtags are in lower case");
    assert(localeObj.privateuse === undefined || IsLowerCase(localeObj.privateuse),
           "If present, privateuse subtag is in lower case");


    // The second step in UTS 35, 3.2.1, is to order all subtags.

    // 1. Any variants are in alphabetical order.
    var variants = localeObj.variants;
    if (variants.length > 0) {
        callFunction(ArraySort, variants);
    }

    // 2. Any extensions are in alphabetical order by their singleton.
    var extensions = localeObj.extensions;
    if (extensions.length > 0) {
        // Extension sequences are sorted by their singleton characters.
        // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
        callFunction(ArraySort, extensions);

        // The last three bullet points in UTS 35, 3.2.1 apply only to Unicode and Transform
        // extensions.
        //
        // 3. All attributes are sorted in alphabetical order.
        //
        // 4. All keywords and tfields are sorted by alphabetical order of their
        //    keys, within their respective extensions.
        //
        // 5. Any type or tfield value "true" is removed.

        for (var i = 0; i < extensions.length; i++) {
            var ext = extensions[i];
            assert(IsLowerCase(ext),
                   "extension subtags must be in lower-case");
            assert(ext[1] === "-",
                   "extension subtags start with a singleton");

            // Canonicalize Unicode locale extension subtag if present.
            if (ext[0] === "u") {
                var {attributes, keywords} = UnicodeExtensionComponents(ext);
                extensions[i] = CanonicalizeUnicodeExtension(attributes, keywords, false);
            }

            // Canonicalize Unicode BCP 47 T extension if present.
            if (ext[0] === "t") {
                var {locale, fields} = TransformExtensionComponents(ext);
                extensions[i] = CanonicalizeTransformExtension(locale, fields);
            }
        }
    }

    // The next two steps in 3.3.1 replace deprecated language and region
    // subtags with their preferred mappings.
    updateLocaleIdMappings(localeObj);

    // The two final steps in 3.3.1, handling irregular grandfathered and
    // private-use only language tags, don't apply, because these two forms
    // can't occur in Unicode BCP 47 locale identifiers.
}

/**
 * Intl.Locale proposal
 *
 * UnicodeExtensionComponents( extension )
 *
 * Returns the components of |extension| where |extension| is a "Unicode locale
 * extension sequence" (ECMA-402, 6.2.1) without the starting separator
 * character.
 */
function UnicodeExtensionComponents(extension) {
    assert(typeof extension === "string", "extension is a String value");

    // Step 1.
    var attributes = [];

    // Step 2.
    var keywords = [];

    // Step 3.
    var isKeyword = false;

    // Step 4.
    var size = extension.length;

    // Step 5.
    // |extension| starts with "u-" instead of "-u-" in our implementation, so
    // we need to initialize |k| with 2 instead of 3.
    assert(callFunction(std_String_startsWith, extension, "u-"),
           "extension starts with 'u-'");
    var k = 2;

    // Step 6.
    var key, value;
    while (k < size) {
        // Step 6.a.
        var e = callFunction(std_String_indexOf, extension, "-", k);

        // Step 6.b.
        var len = (e < 0 ? size : e) - k;

        // Step 6.c.
        var subtag = Substring(extension, k, len);

        // Steps 6.d-e.
        if (!isKeyword) {
            // Step 6.d.
            // NB: Duplicates are handled elsewhere in our implementation.
            if (len !== 2)
                _DefineDataProperty(attributes, attributes.length, subtag);
        } else {
            // Steps 6.e.i-ii.
            if (len === 2) {
                // Step 6.e.i.1.
                // NB: Duplicates are handled elsewhere in our implementation.
                _DefineDataProperty(keywords, keywords.length, {key, value});
            } else {
                // Step 6.e.ii.1.
                if (value !== "")
                    value += "-";

                // Step 6.e.ii.2.
                value += subtag;
            }
        }

        // Step 6.f.
        if (len === 2) {
            // Step 6.f.i.
            isKeyword = true;

            // Step 6.f.ii.
            key = subtag;

            // Step 6.f.iii.
            value = "";
        }

        // Step 6.g.
        k += len + 1;
    }

    // Step 7.
    if (isKeyword) {
        // Step 7.a.
        // NB: Duplicates are handled elsewhere in our implementation.
        _DefineDataProperty(keywords, keywords.length, {key, value});
    }

    // Step 8.
    return {attributes, keywords};
}

/**
 * CanonicalizeUnicodeExtension( attributes, keywords )
 *
 * Canonical syntax per <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All attributes and keywords are in lowercase.
 *   - Note: The parser already converted keywords to lowercase.
 * - All attributes are sorted in alphabetical order.
 * - All keywords are sorted by alphabetical order of their keys.
 * - Any type value "true" is removed.
 *
 * Canonical form:
 * - All keys and types use the canonical form (from the name attribute;
 *   see Section 3.6.4 U Extension Data Files).
 */
function CanonicalizeUnicodeExtension(attributes, keywords, canonicalForm) {
    assert(attributes.length > 0 || keywords.length > 0,
           "unexpected empty Unicode locale extension components");

    // All attributes are sorted in alphabetical order.
    if (attributes.length > 1)
        callFunction(ArraySort, attributes);

    // All keywords are sorted by alphabetical order of keys.
    if (keywords.length > 1) {
        function UnicodeKeySort(left, right) {
            var leftKey = left.key;
            var rightKey = right.key;
            assert(leftKey.length === 2, "left key is a Unicode key");
            assert(rightKey.length === 2, "right key is a Unicode key");

            // Compare both strings using charCodeAt(), because relational
            // string comparison always calls into the VM, whereas charCodeAt
            // can be inlined by Ion.
            var diff = callFunction(std_String_charCodeAt, leftKey, 0) -
                       callFunction(std_String_charCodeAt, rightKey, 0);
            if (diff === 0) {
                diff = callFunction(std_String_charCodeAt, leftKey, 1) -
                       callFunction(std_String_charCodeAt, rightKey, 1);
            }
            return diff;
        }

        callFunction(ArraySort, keywords, UnicodeKeySort);
    }

    var extension = "u";

    // Append all attributes.
    for (var i = 0; i < attributes.length; i++) {
        var attribute = attributes[i];
        assert(attribute === callFunction(std_String_toLowerCase, attribute),
               "Attributes are already canonicalized to lower case");

        // UnicodeExtensionComponents ignores duplicate attributes.
        if (canonicalForm && i > 0 && attributes[i - 1] === attribute) {
            continue;
        }

        extension += "-" + attributes[i];
    }

    // Append all keywords.
    for (var i = 0; i < keywords.length; i++) {
        var {key, value} = keywords[i];
        assert(key === callFunction(std_String_toLowerCase, key) &&
               value === callFunction(std_String_toLowerCase, value),
               "Keywords are already canonicalized to lower case");


        // UnicodeExtensionComponents ignores duplicate keys.
        if (canonicalForm && i > 0 && keywords[i - 1].key === key) {
            continue;
        }

        extension += "-" + key;

        if (canonicalForm &&
            hasOwn(key, deprecatedUnicodeExtensionTypes) &&
            hasOwn(value, deprecatedUnicodeExtensionTypes[key]))
        {
            value = deprecatedUnicodeExtensionTypes[key][value];
            assert(value === callFunction(std_String_toLowerCase, value),
                   "Preferred keyword value is already in lower case");
        }

        // Type value "true" is removed.
        if (value !== "" && value !== "true")
            extension += "-" + value;
    }

    return extension;
}

/**
 * CanonicalizeTransformExtension
 *
 * Canonical form per <https://unicode.org/reports/tr35/#BCP47_T_Extension>:
 *
 * - These subtags are all in lowercase (that is the canonical casing for these
 *   subtags), [...].
 *
 * And per <https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers>:
 *
 * - All keywords and tfields are sorted by alphabetical order of their keys,
 *   within their respective extensions.
 */
function CanonicalizeTransformExtension(localeObj, fields) {
    assert(localeObj !== undefined || fields.length > 0,
           "unexpected empty Transform locale extension components");

    if (fields.length > 0) {
        function TransformKeySort(left, right) {
            var leftKey = left.key;
            var rightKey = right.key;
            assert(leftKey.length === 2, "left key is a Transform key");
            assert(rightKey.length === 2, "right key is a Transform key");

            // Compare both strings using charCodeAt(), because relational
            // string comparison always calls into the VM, whereas charCodeAt
            // can be inlined by Ion.
            var diff = callFunction(std_String_charCodeAt, leftKey, 0) -
                       callFunction(std_String_charCodeAt, rightKey, 0);
            if (diff === 0) {
                diff = callFunction(std_String_charCodeAt, leftKey, 1) -
                       callFunction(std_String_charCodeAt, rightKey, 1);
            }
            return diff;
        }

        callFunction(ArraySort, fields, TransformKeySort);
    }

    var extension = "t";

    // Append the language subtag if present.
    if (localeObj !== undefined) {
        // [1] is a bit unclear whether or not the `tlang` subtag also needs
        // to be canonicalized (and case-adjusted). For now simply append it as
        // is and change it to all lower-case. If we switch to [2], the `tlang`
        // subtag also needs to be canonicalized according to the same rules as
        // `unicode_language_id` subtags are canonicalized. Also see [3].
        //
        // [1] https://unicode.org/reports/tr35/#Language_Tag_to_Locale_Identifier
        // [2] https://unicode.org/reports/tr35/#Canonical_Unicode_Locale_Identifiers
        // [3] https://github.com/tc39/ecma402/issues/330
        var localeStr = StringFromLanguageTagObject(localeObj);
        extension += "-" + callFunction(std_String_toLowerCase, localeStr);
    }

    // Append all fields.
    for (var i = 0; i < fields.length; i++) {
        // UTS 35, 3.2.1 specifies:
        // - Any type or tfield value "true" is removed.
        //
        // But the `tvalue` subtag is mandatory in `tfield: tkey tvalue`, so
        // ignore this apparently invalid part of the UTS 35 specification and
        // simply append all `tfield` subtags.
        var {key, value} = fields[i];
        extension += "-" + key + "-" + value;
    }

    return extension;
}

/**
 * Canonicalizes the given structurally valid BCP 47 language tag, including
 * regularized case of subtags. For example, the language tag
 * Zh-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE, where
 *
 *     Zh             ; 2*3ALPHA
 *     -haNS          ; ["-" script]
 *     -bu            ; ["-" region]
 *     -variant2      ; *("-" variant)
 *     -Variant1
 *     -u-ca-chinese  ; *("-" extension)
 *     -t-Zh-laTN
 *     -x-PRIVATE     ; ["-" privateuse]
 *
 * becomes zh-Hans-MM-variant1-variant2-t-zh-latn-u-ca-chinese-x-private
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.3.
 */
function CanonicalizeLanguageTag(locale) {
    var localeObj = parseLanguageTag(locale);
    assert(localeObj !== null, "CanonicalizeLanguageTag");

    CanonicalizeLanguageTagObject(localeObj);

    return StringFromLanguageTagObject(localeObj);
}

/**
 * Returns the string representation of the given language tag object.
 */
function StringFromLanguageTagObject(localeObj) {
    assert(IsObject(localeObj), "StringFromLanguageTagObject");

    var {
        language,
        script,
        region,
        variants,
        extensions,
        privateuse,
    } = localeObj;

    var canonical = language;

    if (script !== undefined)
        canonical += "-" + script;

    if (region !== undefined)
        canonical += "-" + region;

    if (variants.length > 0)
        canonical += "-" + callFunction(std_Array_join, variants, "-");

    if (extensions.length > 0)
        canonical += "-" + callFunction(std_Array_join, extensions, "-");

    if (privateuse !== undefined)
        canonical += "-" + privateuse;

    return canonical;
}

/**
 * Returns true if the input contains only ASCII alphabetical characters.
 */
function IsASCIIAlphaString(s) {
    assert(typeof s === "string", "IsASCIIAlphaString");

    for (var i = 0; i < s.length; i++) {
        var c = callFunction(std_String_charCodeAt, s, i);
        if (!((0x41 <= c && c <= 0x5A) || (0x61 <= c && c <= 0x7A)))
            return false;
    }
    return true;
}

/**
 * Returns true if the input contains only ASCII digit characters.
 */
function IsASCIIDigitString(s) {
    assert(typeof s === "string", "IsASCIIDigitString");

    for (var i = 0; i < s.length; i++) {
        var c = callFunction(std_String_charCodeAt, s, i);
        if (!(0x30 <= c && c <= 0x39))
            return false;
    }
    return true;
}

/**
 * Validates and canonicalizes the given language tag.
 */
function ValidateAndCanonicalizeLanguageTag(locale) {
    assert(typeof locale === "string", "ValidateAndCanonicalizeLanguageTag");

    // Handle the common case (a standalone language) first.
    // Only the following Unicode BCP 47 locale identifier subset is accepted:
    //   unicode_locale_id = unicode_language_id
    //   unicode_language_id = unicode_language_subtag
    //   unicode_language_subtag = alpha{2,3}
    if (locale.length === 2 || locale.length === 3) {
        if (!IsASCIIAlphaString(locale))
            ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, locale);
        assert(IsStructurallyValidLanguageTag(locale), "2*3ALPHA is a valid language tag");

        // The language subtag is canonicalized to lower case.
        locale = callFunction(std_String_toLowerCase, locale);

        // updateLocaleIdMappings may modify tags containing only |language|
        // subtags, if the language is in |complexLanguageMappings|, so we need
        // to handle that case first.
        if (!hasOwn(locale, complexLanguageMappings)) {
            // Replace deprecated subtags with their preferred values.
            locale = hasOwn(locale, languageMappings)
                     ? languageMappings[locale]
                     : locale;
            assert(locale === CanonicalizeLanguageTag(locale), "expected same canonicalization");

            return locale;
        }
    }

    var localeObj = parseLanguageTag(locale);
    if (localeObj === null)
        ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, locale);

    CanonicalizeLanguageTagObject(localeObj)

    return StringFromLanguageTagObject(localeObj);
}

// The last-ditch locale is used if none of the available locales satisfies a
// request. "en-GB" is used based on the assumptions that English is the most
// common second language, that both en-GB and en-US are normally available in
// an implementation, and that en-GB is more representative of the English used
// in other locales.
function lastDitchLocale() {
    // Per bug 1177929, strings don't clone out of self-hosted code as atoms,
    // breaking IonBuilder::constant.  Put this in a function for now.
    return "en-GB";
}

// Certain old, commonly-used language tags that lack a script, are expected to
// nonetheless imply one.  This object maps these old-style tags to modern
// equivalents.
var oldStyleLanguageTagMappings = {
    "pa-PK": "pa-Arab-PK",
    "zh-CN": "zh-Hans-CN",
    "zh-HK": "zh-Hant-HK",
    "zh-SG": "zh-Hans-SG",
    "zh-TW": "zh-Hant-TW",
};

var localeCandidateCache = {
    runtimeDefaultLocale: undefined,
    candidateDefaultLocale: undefined,
};

var localeCache = {
    runtimeDefaultLocale: undefined,
    defaultLocale: undefined,
};

/**
 * Compute the candidate default locale: the locale *requested* to be used as
 * the default locale.  We'll use it if and only if ICU provides support (maybe
 * fallback support, e.g. supporting "de-ZA" through "de" support implied by a
 * "de-DE" locale).
 */
function DefaultLocaleIgnoringAvailableLocales() {
    const runtimeDefaultLocale = RuntimeDefaultLocale();
    if (runtimeDefaultLocale === localeCandidateCache.runtimeDefaultLocale)
        return localeCandidateCache.candidateDefaultLocale;

    // If we didn't get a cache hit, compute the candidate default locale and
    // cache it.  Fall back on the last-ditch locale when necessary.
    var candidate = parseLanguageTag(runtimeDefaultLocale);
    if (candidate === null) {
        candidate = lastDitchLocale();
    } else {
        CanonicalizeLanguageTagObject(candidate);

        candidate = StringFromLanguageTagObject(candidate);

        // The default locale must be in [[availableLocales]], and that list
        // must not contain any locales with Unicode extension sequences, so
        // remove any present in the candidate.
        candidate = removeUnicodeExtensions(candidate);

        if (hasOwn(candidate, oldStyleLanguageTagMappings))
            candidate = oldStyleLanguageTagMappings[candidate];
    }

    // Cache the candidate locale until the runtime default locale changes.
    localeCandidateCache.candidateDefaultLocale = candidate;
    localeCandidateCache.runtimeDefaultLocale = runtimeDefaultLocale;

    assertIsValidAndCanonicalLanguageTag(candidate, "the candidate locale");
    assert(startOfUnicodeExtensions(candidate) < 0,
           "the candidate must not contain a Unicode extension sequence");

    return candidate;
}

/**
 * Returns the BCP 47 language tag for the host environment's current locale.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.4.
 */
function DefaultLocale() {
    if (IsRuntimeDefaultLocale(localeCache.runtimeDefaultLocale))
        return localeCache.defaultLocale;

    // If we didn't have a cache hit, compute the candidate default locale.
    // Then use it as the actual default locale if ICU supports that locale
    // (perhaps via fallback).  Otherwise use the last-ditch locale.
    var runtimeDefaultLocale = RuntimeDefaultLocale();
    var candidate = DefaultLocaleIgnoringAvailableLocales();
    var locale;
    if (BestAvailableLocaleIgnoringDefault(callFunction(collatorInternalProperties.availableLocales,
                                                        collatorInternalProperties),
                                           candidate) &&
        BestAvailableLocaleIgnoringDefault(callFunction(numberFormatInternalProperties.availableLocales,
                                                        numberFormatInternalProperties),
                                           candidate) &&
        BestAvailableLocaleIgnoringDefault(callFunction(dateTimeFormatInternalProperties.availableLocales,
                                                        dateTimeFormatInternalProperties),
                                           candidate))
    {
        locale = candidate;
    } else {
        locale = lastDitchLocale();
    }

    assertIsValidAndCanonicalLanguageTag(locale, "the computed default locale");
    assert(startOfUnicodeExtensions(locale) < 0,
           "the computed default locale must not contain a Unicode extension sequence");

    localeCache.defaultLocale = locale;
    localeCache.runtimeDefaultLocale = runtimeDefaultLocale;

    return locale;
}

/**
 * Add old-style language tags without script code for locales that in current
 * usage would include a script subtag.  Also add an entry for the last-ditch
 * locale, in case ICU doesn't directly support it (but does support it through
 * fallback, e.g. supporting "en-GB" indirectly using "en" support).
 */
function addSpecialMissingLanguageTags(availableLocales) {
    // Certain old-style language tags lack a script code, but in current usage
    // they *would* include a script code.  Map these over to modern forms.
    var oldStyleLocales = std_Object_getOwnPropertyNames(oldStyleLanguageTagMappings);
    for (var i = 0; i < oldStyleLocales.length; i++) {
        var oldStyleLocale = oldStyleLocales[i];
        if (availableLocales[oldStyleLanguageTagMappings[oldStyleLocale]])
            availableLocales[oldStyleLocale] = true;
    }

    // Also forcibly provide the last-ditch locale.
    var lastDitch = lastDitchLocale();
    assert(lastDitch === "en-GB" && availableLocales.en,
           "shouldn't be a need to add every locale implied by the last-" +
           "ditch locale, merely just the last-ditch locale");
    availableLocales[lastDitch] = true;
}

/**
 * Canonicalizes a locale list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.1.
 */
function CanonicalizeLocaleList(locales) {
    // Step 1.
    if (locales === undefined)
        return [];

    // Step 3 (and the remaining steps).
    if (typeof locales === "string")
        return [ValidateAndCanonicalizeLanguageTag(locales)];

    var unboxedLocale = callFunction(unboxLocaleOrNull, locales);
    if (unboxedLocale !== null)
        return [StringFromLanguageTagObject(unboxedLocale.locale)]

    // Step 2.
    var seen = [];

    // Step 4.
    var O = ToObject(locales);

    // Step 5.
    var len = ToLength(O.length);

    // Step 6.
    var k = 0;

    // Step 7.
    while (k < len) {
        // Steps 7.a-c.
        if (k in O) {
            // Step 7.c.i.
            var kValue = O[k];

            // Step 7.c.ii.
            if (!(typeof kValue === "string" || IsObject(kValue)))
                ThrowTypeError(JSMSG_INVALID_LOCALES_ELEMENT);

            // Steps 7.c.iii-iv.
            var unboxedLocale = callFunction(unboxLocaleOrNull, kValue);
            var tag = unboxedLocale !== null
                      ? StringFromLanguageTagObject(unboxedLocale.locale)
                      : ValidateAndCanonicalizeLanguageTag(ToString(kValue));

            // Step 7.c.v.
            if (callFunction(ArrayIndexOf, seen, tag) === -1)
                _DefineDataProperty(seen, seen.length, tag);
        }

        // Step 7.d.
        k++;
    }

    // Step 8.
    return seen;
}

function BestAvailableLocaleHelper(availableLocales, locale, considerDefaultLocale) {
    assertIsValidAndCanonicalLanguageTag(locale, "BestAvailableLocale locale");
    assert(startOfUnicodeExtensions(locale) < 0, "locale must contain no Unicode extensions");

    // In the spec, [[availableLocales]] is formally a list of all available
    // locales.  But in our implementation, it's an *incomplete* list, not
    // necessarily including the default locale (and all locales implied by it,
    // e.g. "de" implied by "de-CH"), if that locale isn't in every
    // [[availableLocales]] list (because that locale is supported through
    // fallback, e.g. "de-CH" supported through "de").
    //
    // If we're considering the default locale, augment the spec loop with
    // additional checks to also test whether the current prefix is a prefix of
    // the default locale.

    var defaultLocale;
    if (considerDefaultLocale)
        defaultLocale = DefaultLocale();

    var candidate = locale;
    while (true) {
        if (availableLocales[candidate])
            return candidate;

        if (considerDefaultLocale && candidate.length <= defaultLocale.length) {
            if (candidate === defaultLocale)
                return candidate;
            if (callFunction(std_String_startsWith, defaultLocale, candidate + "-"))
                return candidate;
        }

        var pos = callFunction(std_String_lastIndexOf, candidate, "-");
        if (pos === -1)
            return undefined;

        if (pos >= 2 && candidate[pos - 2] === "-")
            pos -= 2;

        candidate = callFunction(String_substring, candidate, 0, pos);
    }
}

/**
 * Compares a BCP 47 language tag against the locales in availableLocales
 * and returns the best available match. Uses the fallback
 * mechanism of RFC 4647, section 3.4.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.2.
 * Spec: RFC 4647, section 3.4.
 */
function BestAvailableLocale(availableLocales, locale) {
    return BestAvailableLocaleHelper(availableLocales, locale, true);
}

/**
 * Identical to BestAvailableLocale, but does not consider the default locale
 * during computation.
 */
function BestAvailableLocaleIgnoringDefault(availableLocales, locale) {
    return BestAvailableLocaleHelper(availableLocales, locale, false);
}

/**
 * Compares a BCP 47 language priority list against the set of locales in
 * availableLocales and determines the best available language to meet the
 * request. Options specified through Unicode extension subsequences are
 * ignored in the lookup, but information about such subsequences is returned
 * separately.
 *
 * This variant is based on the Lookup algorithm of RFC 4647 section 3.4.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.3.
 * Spec: RFC 4647, section 3.4.
 */
function LookupMatcher(availableLocales, requestedLocales) {
    // Step 1.
    var result = new Record();

    // Step 2.
    for (var i = 0; i < requestedLocales.length; i++) {
        var locale = requestedLocales[i];

        // Step 2.a.
        var noExtensionsLocale = removeUnicodeExtensions(locale);

        // Step 2.b.
        var availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);

        // Step 2.c.
        if (availableLocale !== undefined) {
            // Step 2.c.i.
            result.locale = availableLocale;

            // Step 2.c.ii.
            if (locale !== noExtensionsLocale)
                result.extension = getUnicodeExtensions(locale);

            // Step 2.c.iii.
            return result;
        }
    }

    // Steps 3-4.
    result.locale = DefaultLocale();

    // Step 5.
    return result;
}

/**
 * Compares a BCP 47 language priority list against the set of locales in
 * availableLocales and determines the best available language to meet the
 * request. Options specified through Unicode extension subsequences are
 * ignored in the lookup, but information about such subsequences is returned
 * separately.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.4.
 */
function BestFitMatcher(availableLocales, requestedLocales) {
    // this implementation doesn't have anything better
    return LookupMatcher(availableLocales, requestedLocales);
}

/**
 * Returns the Unicode extension value subtags for the requested key subtag.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.5.
 */
function UnicodeExtensionValue(extension, key) {
    assert(typeof extension === "string", "extension is a string value");
    assert(callFunction(std_String_startsWith, extension, "-u-") &&
           getUnicodeExtensions("und" + extension) === extension,
           "extension is a Unicode extension subtag");
    assert(typeof key === "string", "key is a string value");

    // Step 1.
    assert(key.length === 2, "key is a Unicode extension key subtag");

    // Step 2.
    var size = extension.length;

    // Step 3.
    var searchValue = "-" + key + "-";

    // Step 4.
    var pos = callFunction(std_String_indexOf, extension, searchValue);

    // Step 5.
    if (pos !== -1) {
        // Step 5.a.
        var start = pos + 4;

        // Step 5.b.
        var end = start;

        // Step 5.c.
        var k = start;

        // Steps 5.d-e.
        while (true) {
            // Step 5.e.i.
            var e = callFunction(std_String_indexOf, extension, "-", k);

            // Step 5.e.ii.
            var len = e === -1 ? size - k : e - k;

            // Step 5.e.iii.
            if (len === 2)
                break;

            // Step 5.e.iv.
            if (e === -1) {
                end = size;
                break;
            }

            // Step 5.e.v.
            end = e;
            k = e + 1;
        }

        // Step 5.f.
        return callFunction(String_substring, extension, start, end);
    }

    // Step 6.
    searchValue = "-" + key;

    // Steps 7-8.
    if (callFunction(std_String_endsWith, extension, searchValue))
        return "";

    // Step 9 (implicit).
}

/**
 * Compares a BCP 47 language priority list against availableLocales and
 * determines the best available language to meet the request. Options specified
 * through Unicode extension subsequences are negotiated separately, taking the
 * caller's relevant extensions and locale data as well as client-provided
 * options into consideration.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.6.
 */
function ResolveLocale(availableLocales, requestedLocales, options, relevantExtensionKeys, localeData) {
    // Steps 1-3.
    var matcher = options.localeMatcher;
    var r = (matcher === "lookup")
            ? LookupMatcher(availableLocales, requestedLocales)
            : BestFitMatcher(availableLocales, requestedLocales);

    // Step 4.
    var foundLocale = r.locale;
    var extension = r.extension;

    // Step 5.
    var result = new Record();

    // Step 6.
    result.dataLocale = foundLocale;

    // Step 7.
    var supportedExtension = "-u";

    // In this implementation, localeData is a function, not an object.
    var localeDataProvider = localeData();

    // Step 8.
    for (var i = 0; i < relevantExtensionKeys.length; i++) {
        var key = relevantExtensionKeys[i];

        // Steps 8.a-h (The locale data is only computed when needed).
        var keyLocaleData = undefined;
        var value = undefined;

        // Locale tag may override.

        // Step 8.g.
        var supportedExtensionAddition = "";

        // Step 8.h.
        if (extension !== undefined) {
            // Step 8.h.i.
            var requestedValue = UnicodeExtensionValue(extension, key);

            // Step 8.h.ii.
            if (requestedValue !== undefined) {
                // Steps 8.a-d.
                keyLocaleData = callFunction(localeDataProvider[key], null, foundLocale);

                // Step 8.h.ii.1.
                if (requestedValue !== "") {
                    // Step 8.h.ii.1.a.
                    if (callFunction(ArrayIndexOf, keyLocaleData, requestedValue) !== -1) {
                        value = requestedValue;
                        supportedExtensionAddition = "-" + key + "-" + value;
                    }
                } else {
                    // Step 8.h.ii.2.

                    // According to the LDML spec, if there's no type value,
                    // and true is an allowed value, it's used.
                    if (callFunction(ArrayIndexOf, keyLocaleData, "true") !== -1) {
                        value = "true";
                        supportedExtensionAddition = "-" + key;
                    }
                }
            }
        }

        // Options override all.

        // Step 8.i.i.
        var optionsValue = options[key];

        // Step 8.i.ii.
        assert(typeof optionsValue === "string" ||
               optionsValue === undefined ||
               optionsValue === null,
               "unexpected type for options value");

        // Steps 8.i, 8.i.iii.1.
        if (optionsValue !== undefined && optionsValue !== value) {
            // Steps 8.a-d.
            if (keyLocaleData === undefined)
                keyLocaleData = callFunction(localeDataProvider[key], null, foundLocale);

            // Step 8.i.iii.
            if (callFunction(ArrayIndexOf, keyLocaleData, optionsValue) !== -1) {
                value = optionsValue;
                supportedExtensionAddition = "";
            }
        }

        // Locale data provides default value.
        if (value === undefined) {
            // Steps 8.a-f.
            value = keyLocaleData === undefined
                    ? callFunction(localeDataProvider.default[key], null, foundLocale)
                    : keyLocaleData[0];
        }

        // Step 8.j.
        assert(typeof value === "string" || value === null, "unexpected locale data value");
        result[key] = value;

        // Step 8.k.
        supportedExtension += supportedExtensionAddition;
    }

    // Step 9.
    if (supportedExtension.length > 2) {
        assert(!callFunction(std_String_startsWith, foundLocale, "x-"),
               "unexpected privateuse-only locale returned from ICU");

        // Step 9.a.
        var privateIndex = callFunction(std_String_indexOf, foundLocale, "-x-");

        // Steps 9.b-c.
        if (privateIndex === -1) {
            foundLocale += supportedExtension;
        } else {
            var preExtension = callFunction(String_substring, foundLocale, 0, privateIndex);
            var postExtension = callFunction(String_substring, foundLocale, privateIndex);
            foundLocale = preExtension + supportedExtension + postExtension;
        }

        // Steps 9.d-e (Step 9.e is not required in this implementation,
        // because we don't canonicalize Unicode extension subtags).
        assertIsValidAndCanonicalLanguageTag(foundLocale, "locale after concatenation");
    }

    // Step 10.
    result.locale = foundLocale;

    // Step 11.
    return result;
}

/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.7.
 */
function LookupSupportedLocales(availableLocales, requestedLocales) {
    // Step 1.
    var subset = [];

    // Step 2.
    for (var i = 0; i < requestedLocales.length; i++) {
        var locale = requestedLocales[i];

        // Step 2.a.
        var noExtensionsLocale = removeUnicodeExtensions(locale);

        // Step 2.b.
        var availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);

        // Step 2.c.
        if (availableLocale !== undefined)
            _DefineDataProperty(subset, subset.length, locale);
    }

    // Step 3.
    return subset;
}

/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.8.
 */
function BestFitSupportedLocales(availableLocales, requestedLocales) {
    // don't have anything better
    return LookupSupportedLocales(availableLocales, requestedLocales);
}

/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.9.
 */
function SupportedLocales(availableLocales, requestedLocales, options) {
    // Step 1.
    var matcher;
    if (options !== undefined) {
        // Step 1.a.
        options = ToObject(options);

        // Step 1.b
        matcher = options.localeMatcher;
        if (matcher !== undefined) {
            matcher = ToString(matcher);
            if (matcher !== "lookup" && matcher !== "best fit")
                ThrowRangeError(JSMSG_INVALID_LOCALE_MATCHER, matcher);
        }
    }

    // Steps 2-5.
    return (matcher === undefined || matcher === "best fit")
           ? BestFitSupportedLocales(availableLocales, requestedLocales)
           : LookupSupportedLocales(availableLocales, requestedLocales);
}

/**
 * Extracts a property value from the provided options object, converts it to
 * the required type, checks whether it is one of a list of allowed values,
 * and fills in a fallback value if necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.10.
 */
function GetOption(options, property, type, values, fallback) {
    // Step 1.
    var value = options[property];

    // Step 2.
    if (value !== undefined) {
        // Steps 2.a-c.
        if (type === "boolean")
            value = ToBoolean(value);
        else if (type === "string")
            value = ToString(value);
        else
            assert(false, "GetOption");

        // Step 2.d.
        if (values !== undefined && callFunction(ArrayIndexOf, values, value) === -1)
            ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, property, value);

        // Step 2.e.
        return value;
    }

    // Step 3.
    return fallback;
}

/**
 * The abstract operation DefaultNumberOption converts value to a Number value,
 * checks whether it is in the allowed range, and fills in a fallback value if
 * necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.11.
 */
function DefaultNumberOption(value, minimum, maximum, fallback) {
    assert(typeof minimum === "number" && (minimum | 0) === minimum, "DefaultNumberOption");
    assert(typeof maximum === "number" && (maximum | 0) === maximum, "DefaultNumberOption");
    assert(typeof fallback === "number" && (fallback | 0) === fallback, "DefaultNumberOption");
    assert(minimum <= fallback && fallback <= maximum, "DefaultNumberOption");

    // Step 1.
    if (value !== undefined) {
        value = ToNumber(value);
        if (Number_isNaN(value) || value < minimum || value > maximum)
            ThrowRangeError(JSMSG_INVALID_DIGITS_VALUE, value);

        // Apply bitwise-or to convert -0 to +0 per ES2017, 5.2 and to ensure
        // the result is an int32 value.
        return std_Math_floor(value) | 0;
    }

    // Step 2.
    return fallback;
}

/**
 * Extracts a property value from the provided options object, converts it to a
 * Number value, checks whether it is in the allowed range, and fills in a
 * fallback value if necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.12.
 */
function GetNumberOption(options, property, minimum, maximum, fallback) {
    // Steps 1-2.
    return DefaultNumberOption(options[property], minimum, maximum, fallback);
}

// Symbols in the self-hosting compartment can't be cloned, use a separate
// object to hold the actual symbol value.
// TODO: Can we add support to clone symbols?
var intlFallbackSymbolHolder = { value: undefined };

/**
 * The [[FallbackSymbol]] symbol of the %Intl% intrinsic object.
 *
 * This symbol is used to implement the legacy constructor semantics for
 * Intl.DateTimeFormat and Intl.NumberFormat.
 */
function intlFallbackSymbol() {
    var fallbackSymbol = intlFallbackSymbolHolder.value;
    if (!fallbackSymbol) {
        fallbackSymbol = std_Symbol("IntlLegacyConstructedSymbol");
        intlFallbackSymbolHolder.value = fallbackSymbol;
    }
    return fallbackSymbol;
}

/**
 * Initializes the INTL_INTERNALS_OBJECT_SLOT of the given object.
 */
function initializeIntlObject(obj, type, lazyData) {
    assert(IsObject(obj), "Non-object passed to initializeIntlObject");
    assert((type === "Collator" && GuardToCollator(obj) !== null) ||
           (type === "DateTimeFormat" && GuardToDateTimeFormat(obj) !== null) ||
           (type === "NumberFormat" && GuardToNumberFormat(obj) !== null) ||
           (type === "PluralRules" && GuardToPluralRules(obj) !== null) ||
           (type === "RelativeTimeFormat" && GuardToRelativeTimeFormat(obj) !== null),
           "type must match the object's class");
    assert(IsObject(lazyData), "non-object lazy data");

    // The meaning of an internals object for an object |obj| is as follows.
    //
    // The .type property indicates the type of Intl object that |obj| is:
    // "Collator", "DateTimeFormat", "NumberFormat", or "PluralRules" (likely
    // with more coming in future Intl specs).
    //
    // The .lazyData property stores information needed to compute -- without
    // observable side effects -- the actual internal Intl properties of
    // |obj|.  If it is non-null, then the actual internal properties haven't
    // been computed, and .lazyData must be processed by
    // |setInternalProperties| before internal Intl property values are
    // available.  If it is null, then the .internalProps property contains an
    // object whose properties are the internal Intl properties of |obj|.

    var internals = std_Object_create(null);
    internals.type = type;
    internals.lazyData = lazyData;
    internals.internalProps = null;

    assert(UnsafeGetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT) === undefined,
           "Internal slot already initialized?");
    UnsafeSetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT, internals);
}

/**
 * Set the internal properties object for an |internals| object previously
 * associated with lazy data.
 */
function setInternalProperties(internals, internalProps) {
    assert(IsObject(internals.lazyData), "lazy data must exist already");
    assert(IsObject(internalProps), "internalProps argument should be an object");

    // Set in reverse order so that the .lazyData nulling is a barrier.
    internals.internalProps = internalProps;
    internals.lazyData = null;
}

/**
 * Get the existing internal properties out of a non-newborn |internals|, or
 * null if none have been computed.
 */
function maybeInternalProperties(internals) {
    assert(IsObject(internals), "non-object passed to maybeInternalProperties");
    var lazyData = internals.lazyData;
    if (lazyData)
        return null;
    assert(IsObject(internals.internalProps), "missing lazy data and computed internals");
    return internals.internalProps;
}

/**
 * Return |obj|'s internals object (*not* the object holding its internal
 * properties!), with structure specified above.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.
 * Spec: ECMAScript Internationalization API Specification, 11.3.
 * Spec: ECMAScript Internationalization API Specification, 12.3.
 */
function getIntlObjectInternals(obj) {
    assert(IsObject(obj), "getIntlObjectInternals called with non-Object");
    assert(GuardToCollator(obj) !== null || GuardToDateTimeFormat(obj) !== null ||
           GuardToNumberFormat(obj) !== null || GuardToPluralRules(obj) !== null ||
           GuardToRelativeTimeFormat(obj) !== null,
           "getIntlObjectInternals called with non-Intl object");

    var internals = UnsafeGetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT);

    assert(IsObject(internals), "internals not an object");
    assert(hasOwn("type", internals), "missing type");
    assert((internals.type === "Collator" && GuardToCollator(obj) !== null) ||
           (internals.type === "DateTimeFormat" && GuardToDateTimeFormat(obj) !== null) ||
           (internals.type === "NumberFormat" && GuardToNumberFormat(obj) !== null) ||
           (internals.type === "PluralRules" && GuardToPluralRules(obj) !== null) ||
           (internals.type === "RelativeTimeFormat" && GuardToRelativeTimeFormat(obj) !== null),
           "type must match the object's class");
    assert(hasOwn("lazyData", internals), "missing lazyData");
    assert(hasOwn("internalProps", internals), "missing internalProps");

    return internals;
}

/**
 * Get the internal properties of known-Intl object |obj|.  For use only by
 * C++ code that knows what it's doing!
 */
function getInternals(obj) {
    var internals = getIntlObjectInternals(obj);

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    var type = internals.type;
    if (type === "Collator")
        internalProps = resolveCollatorInternals(internals.lazyData);
    else if (type === "DateTimeFormat")
        internalProps = resolveDateTimeFormatInternals(internals.lazyData);
    else if (type === "NumberFormat")
        internalProps = resolveNumberFormatInternals(internals.lazyData);
    else
        internalProps = resolvePluralRulesInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}
