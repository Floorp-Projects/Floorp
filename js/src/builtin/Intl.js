/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Portions Copyright Norbert Lindenberg 2011-2012. */

/*global JSMSG_INTL_OBJECT_NOT_INITED: false, JSMSG_INVALID_LOCALES_ELEMENT: false,
         JSMSG_INVALID_LANGUAGE_TAG: false, JSMSG_INVALID_LOCALE_MATCHER: false,
         JSMSG_INVALID_OPTION_VALUE: false, JSMSG_INVALID_DIGITS_VALUE: false,
         JSMSG_INVALID_CURRENCY_CODE: false,
         JSMSG_UNDEFINED_CURRENCY: false, JSMSG_INVALID_TIME_ZONE: false,
         JSMSG_DATE_NOT_FINITE: false, JSMSG_INVALID_KEYS_TYPE: false,
         JSMSG_INVALID_KEY: false,
         intl_Collator_availableLocales: false,
         intl_availableCollations: false,
         intl_CompareStrings: false,
         intl_NumberFormat_availableLocales: false,
         intl_numberingSystem: false,
         intl_FormatNumber: false,
         intl_DateTimeFormat_availableLocales: false,
         intl_availableCalendars: false,
         intl_patternForSkeleton: false,
         intl_FormatDateTime: false,
         intl_SelectPluralRule: false,
         intl_GetPluralCategories: false,
         intl_GetCalendarInfo: false,
*/

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */


/********** Locales, Time Zones, and Currencies **********/


/**
 * Convert s to upper case, but limited to characters a-z.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.1.
 */
function toASCIIUpperCase(s) {
    assert(typeof s === "string", "toASCIIUpperCase");

    // String.prototype.toUpperCase may map non-ASCII characters into ASCII,
    // so go character by character (actually code unit by code unit, but
    // since we only care about ASCII characters here, that's OK).
    var result = "";
    for (var i = 0; i < s.length; i++) {
        var c = callFunction(std_String_charCodeAt, s, i);
        result += (0x61 <= c && c <= 0x7A)
                  ? callFunction(std_String_fromCharCode, null, c & ~0x20)
                  : s[i];
    }
    return result;
}

/**
 * Holder object for encapsulating regexp instances.
 *
 * Regular expression instances should be created after the initialization of
 * self-hosted global.
 */
var internalIntlRegExps = std_Object_create(null);
internalIntlRegExps.unicodeLocaleExtensionSequenceRE = null;
internalIntlRegExps.languageTagRE = null;
internalIntlRegExps.duplicateVariantRE = null;
internalIntlRegExps.duplicateSingletonRE = null;
internalIntlRegExps.isWellFormedCurrencyCodeRE = null;
internalIntlRegExps.currencyDigitsRE = null;

/**
 * Regular expression matching a "Unicode locale extension sequence", which the
 * specification defines as: "any substring of a language tag that starts with
 * a separator '-' and the singleton 'u' and includes the maximum sequence of
 * following non-singleton subtags and their preceding '-' separators."
 *
 * Alternatively, this may be defined as: the components of a language tag that
 * match the extension production in RFC 5646, where the singleton component is
 * "u".
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.1.
 */
function getUnicodeLocaleExtensionSequenceRE() {
    return internalIntlRegExps.unicodeLocaleExtensionSequenceRE ||
           (internalIntlRegExps.unicodeLocaleExtensionSequenceRE =
            RegExpCreate("-u(?:-[a-z0-9]{2,8})+"));
}


/**
 * Removes Unicode locale extension sequences from the given language tag.
 */
function removeUnicodeExtensions(locale) {
    // A wholly-privateuse locale has no extension sequences.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return locale;

    // Otherwise, split on "-x-" marking the start of any privateuse component.
    // Replace Unicode locale extension sequences in the left half, and return
    // the concatenation.
    var pos = callFunction(std_String_indexOf, locale, "-x-");
    if (pos < 0)
        pos = locale.length;

    var left = callFunction(String_substring, locale, 0, pos);
    var right = callFunction(String_substring, locale, pos);

    var unicodeLocaleExtensionSequenceRE = getUnicodeLocaleExtensionSequenceRE();
    var extensions = regexp_exec_no_statics(unicodeLocaleExtensionSequenceRE, left);
    if (extensions !== null) {
        left = callFunction(String_substring, left, 0, extensions.index) +
               callFunction(String_substring, left, extensions.index + extensions[0].length);
    }

    var combined = left + right;
    assert(IsStructurallyValidLanguageTag(combined), "recombination produced an invalid language tag");
    assert(function() {
        var uindex = callFunction(std_String_indexOf, combined, "-u-");
        if (uindex < 0)
            return true;
        var xindex = callFunction(std_String_indexOf, combined, "-x-");
        return xindex > 0 && xindex < uindex;
    }(), "recombination failed to remove all Unicode locale extension sequences");

    return combined;
}


/**
 * Regular expression defining BCP 47 language tags.
 *
 * Spec: RFC 5646 section 2.1.
 */
function getLanguageTagRE() {
    if (internalIntlRegExps.languageTagRE)
        return internalIntlRegExps.languageTagRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // regular       = "art-lojban"        ; these tags match the 'langtag'
    //               / "cel-gaulish"       ; production, but their subtags
    //               / "no-bok"            ; are not extended language
    //               / "no-nyn"            ; or variant subtags: their meaning
    //               / "zh-guoyu"          ; is defined by their registration
    //               / "zh-hakka"          ; and all of these are deprecated
    //               / "zh-min"            ; in favor of a more modern
    //               / "zh-min-nan"        ; subtag or sequence of subtags
    //               / "zh-xiang"
    var regular = "(?:art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|zh-min|zh-min-nan|zh-xiang)";
    // irregular     = "en-GB-oed"         ; irregular tags do not match
    //                / "i-ami"             ; the 'langtag' production and
    //                / "i-bnn"             ; would not otherwise be
    //                / "i-default"         ; considered 'well-formed'
    //                / "i-enochian"        ; These tags are all valid,
    //                / "i-hak"             ; but most are deprecated
    //                / "i-klingon"         ; in favor of more modern
    //                / "i-lux"             ; subtags or subtag
    //                / "i-mingo"           ; combination
    //                / "i-navajo"
    //                / "i-pwn"
    //                / "i-tao"
    //                / "i-tay"
    //                / "i-tsu"
    //                / "sgn-BE-FR"
    //                / "sgn-BE-NL"
    //                / "sgn-CH-DE"
    var irregular = "(?:en-GB-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|i-tsu|sgn-BE-FR|sgn-BE-NL|sgn-CH-DE)";
    // grandfathered = irregular           ; non-redundant tags registered
    //               / regular             ; during the RFC 3066 era
    var grandfathered = "(?:" + irregular + "|" + regular + ")";
    // privateuse    = "x" 1*("-" (1*8alphanum))
    var privateuse = "(?:x(?:-[a-z0-9]{1,8})+)";
    // singleton     = DIGIT               ; 0 - 9
    //               / %x41-57             ; A - W
    //               / %x59-5A             ; Y - Z
    //               / %x61-77             ; a - w
    //               / %x79-7A             ; y - z
    var singleton = "(?:" + DIGIT + "|[A-WY-Za-wy-z])";
    // extension     = singleton 1*("-" (2*8alphanum))
    var extension = "(?:" + singleton + "(?:-" + alphanum + "{2,8})+)";
    // variant       = 5*8alphanum         ; registered variants
    //               / (DIGIT 3alphanum)
    var variant = "(?:" + alphanum + "{5,8}|(?:" + DIGIT + alphanum + "{3}))";
    // region        = 2ALPHA              ; ISO 3166-1 code
    //               / 3DIGIT              ; UN M.49 code
    var region = "(?:" + ALPHA + "{2}|" + DIGIT + "{3})";
    // script        = 4ALPHA              ; ISO 15924 code
    var script = "(?:" + ALPHA + "{4})";
    // extlang       = 3ALPHA              ; selected ISO 639 codes
    //                 *2("-" 3ALPHA)      ; permanently reserved
    var extlang = "(?:" + ALPHA + "{3}(?:-" + ALPHA + "{3}){0,2})";
    // language      = 2*3ALPHA            ; shortest ISO 639 code
    //                 ["-" extlang]       ; sometimes followed by
    //                                     ; extended language subtags
    //               / 4ALPHA              ; or reserved for future use
    //               / 5*8ALPHA            ; or registered language subtag
    var language = "(?:" + ALPHA + "{2,3}(?:-" + extlang + ")?|" + ALPHA + "{4}|" + ALPHA + "{5,8})";
    // langtag       = language
    //                 ["-" script]
    //                 ["-" region]
    //                 *("-" variant)
    //                 *("-" extension)
    //                 ["-" privateuse]
    var langtag = language + "(?:-" + script + ")?(?:-" + region + ")?(?:-" +
                  variant + ")*(?:-" + extension + ")*(?:-" + privateuse + ")?";
    // Language-Tag  = langtag             ; normal language tags
    //               / privateuse          ; private use tag
    //               / grandfathered       ; grandfathered tags
    var languageTag = "^(?:" + langtag + "|" + privateuse + "|" + grandfathered + ")$";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).
    return (internalIntlRegExps.languageTagRE = RegExpCreate(languageTag, "i"));
}


function getDuplicateVariantRE() {
    if (internalIntlRegExps.duplicateVariantRE)
        return internalIntlRegExps.duplicateVariantRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // variant       = 5*8alphanum         ; registered variants
    //               / (DIGIT 3alphanum)
    var variant = "(?:" + alphanum + "{5,8}|(?:" + DIGIT + alphanum + "{3}))";

    // Match a langtag that contains a duplicate variant.
    var duplicateVariant =
        // Match everything in a langtag prior to any variants, and maybe some
        // of the variants as well (which makes this pattern inefficient but
        // not wrong, for our purposes);
        "^(?:" + alphanum + "{2,8}-)+" +
        // a variant, parenthesised so that we can refer back to it later;
        "(" + variant + ")-" +
        // zero or more subtags at least two characters long (thus stopping
        // before extension and privateuse components);
        "(?:" + alphanum + "{2,8}-)*" +
        // and the same variant again
        "\\1" +
        // ...but not followed by any characters that would turn it into a
        // different subtag.
        "(?!" + alphanum + ")";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).  Using
    // character classes covering both upper- and lower-case characters nearly
    // addresses this -- but for the possibility of variant repetition with
    // differing case, e.g. "en-variant-Variant".  Use a case-insensitive
    // regular expression to address this.  (Note that there's no worry about
    // case transformation accepting invalid characters here: users have
    // already verified the string is alphanumeric Latin plus "-".)
    return (internalIntlRegExps.duplicateVariantRE = RegExpCreate(duplicateVariant, "i"));
}


function getDuplicateSingletonRE() {
    if (internalIntlRegExps.duplicateSingletonRE)
        return internalIntlRegExps.duplicateSingletonRE;

    // RFC 5234 section B.1
    // ALPHA          =  %x41-5A / %x61-7A   ; A-Z / a-z
    var ALPHA = "[a-zA-Z]";
    // DIGIT          =  %x30-39
    //                        ; 0-9
    var DIGIT = "[0-9]";

    // RFC 5646 section 2.1
    // alphanum      = (ALPHA / DIGIT)     ; letters and numbers
    var alphanum = "(?:" + ALPHA + "|" + DIGIT + ")";
    // singleton     = DIGIT               ; 0 - 9
    //               / %x41-57             ; A - W
    //               / %x59-5A             ; Y - Z
    //               / %x61-77             ; a - w
    //               / %x79-7A             ; y - z
    var singleton = "(?:" + DIGIT + "|[A-WY-Za-wy-z])";

    // Match a langtag that contains a duplicate singleton.
    var duplicateSingleton =
        // Match a singleton subtag, parenthesised so that we can refer back to
        // it later;
        "-(" + singleton + ")-" +
        // then zero or more subtags;
        "(?:" + alphanum + "+-)*" +
        // and the same singleton again
        "\\1" +
        // ...but not followed by any characters that would turn it into a
        // different subtag.
        "(?!" + alphanum + ")";

    // Language tags are case insensitive (RFC 5646 section 2.1.1).  Using
    // character classes covering both upper- and lower-case characters nearly
    // addresses this -- but for the possibility of singleton repetition with
    // differing case, e.g. "en-u-foo-U-foo".  Use a case-insensitive regular
    // expression to address this.  (Note that there's no worry about case
    // transformation accepting invalid characters here: users have already
    // verified the string is alphanumeric Latin plus "-".)
    return (internalIntlRegExps.duplicateSingletonRE = RegExpCreate(duplicateSingleton, "i"));
}


/**
 * Verifies that the given string is a well-formed BCP 47 language tag
 * with no duplicate variant or singleton subtags.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.2.
 */
function IsStructurallyValidLanguageTag(locale) {
    assert(typeof locale === "string", "IsStructurallyValidLanguageTag");
    var languageTagRE = getLanguageTagRE();
    if (!regexp_test_no_statics(languageTagRE, locale))
        return false;

    // Before checking for duplicate variant or singleton subtags with
    // regular expressions, we have to get private use subtag sequences
    // out of the picture.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return true;
    var pos = callFunction(std_String_indexOf, locale, "-x-");
    if (pos !== -1)
        locale = callFunction(String_substring, locale, 0, pos);

    // Check for duplicate variant or singleton subtags.
    var duplicateVariantRE = getDuplicateVariantRE();
    var duplicateSingletonRE = getDuplicateSingletonRE();
    return !regexp_test_no_statics(duplicateVariantRE, locale) &&
           !regexp_test_no_statics(duplicateSingletonRE, locale);
}


/**
 * Canonicalizes the given structurally valid BCP 47 language tag, including
 * regularized case of subtags. For example, the language tag
 * Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE, where
 *
 *     Zh             ; 2*3ALPHA
 *     -NAN           ; ["-" extlang]
 *     -haNS          ; ["-" script]
 *     -bu            ; ["-" region]
 *     -variant2      ; *("-" variant)
 *     -Variant1
 *     -u-ca-chinese  ; *("-" extension)
 *     -t-Zh-laTN
 *     -x-PRIVATE     ; ["-" privateuse]
 *
 * becomes nan-Hans-mm-variant2-variant1-t-zh-latn-u-ca-chinese-x-private
 *
 * Spec: ECMAScript Internationalization API Specification, 6.2.3.
 * Spec: RFC 5646, section 4.5.
 */
function CanonicalizeLanguageTag(locale) {
    assert(IsStructurallyValidLanguageTag(locale), "CanonicalizeLanguageTag");

    // The input
    // "Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE"
    // will be used throughout this method to illustrate how it works.

    // Language tags are compared and processed case-insensitively, so
    // technically it's not necessary to adjust case. But for easier processing,
    // and because the canonical form for most subtags is lower case, we start
    // with lower case for all.
    // "Zh-NAN-haNS-bu-variant2-Variant1-u-ca-chinese-t-Zh-laTN-x-PRIVATE" ->
    // "zh-nan-hans-bu-variant2-variant1-u-ca-chinese-t-zh-latn-x-private"
    locale = callFunction(std_String_toLowerCase, locale);

    // Handle mappings for complete tags.
    if (hasOwn(locale, langTagMappings))
        return langTagMappings[locale];

    var subtags = StringSplitString(locale, "-");
    var i = 0;

    // Handle the standard part: All subtags before the first singleton or "x".
    // "zh-nan-hans-bu-variant2-variant1"
    while (i < subtags.length) {
        var subtag = subtags[i];

        // If we reach the start of an extension sequence or private use part,
        // we're done with this loop. We have to check for i > 0 because for
        // irregular language tags, such as i-klingon, the single-character
        // subtag "i" is not the start of an extension sequence.
        // In the example, we break at "u".
        if (subtag.length === 1 && (i > 0 || subtag === "x"))
            break;

        if (i !== 0) {
            if (subtag.length === 4) {
                // 4-character subtags that are not in initial position are
                // script codes; their first character needs to be capitalized.
                // "hans" -> "Hans"
                subtag = callFunction(std_String_toUpperCase, subtag[0]) +
                         callFunction(String_substring, subtag, 1);
            } else if (subtag.length === 2) {
                // 2-character subtags that are not in initial position are
                // region codes; they need to be upper case. "bu" -> "BU"
                subtag = callFunction(std_String_toUpperCase, subtag);
            }
        }
        if (hasOwn(subtag, langSubtagMappings)) {
            // Replace deprecated subtags with their preferred values.
            // "BU" -> "MM"
            // This has to come after we capitalize region codes because
            // otherwise some language and region codes could be confused.
            // For example, "in" is an obsolete language code for Indonesian,
            // but "IN" is the country code for India.
            // Note that the script generating langSubtagMappings makes sure
            // that no regular subtag mapping will replace an extlang code.
            subtag = langSubtagMappings[subtag];
        } else if (hasOwn(subtag, extlangMappings)) {
            // Replace deprecated extlang subtags with their preferred values,
            // and remove the preceding subtag if it's a redundant prefix.
            // "zh-nan" -> "nan"
            // Note that the script generating extlangMappings makes sure that
            // no extlang mapping will replace a normal language code.
            // The preferred value for all current deprecated extlang subtags
            // is equal to the extlang subtag, so we only need remove the
            // redundant prefix to get the preferred value.
            if (i === 1 && extlangMappings[subtag] === subtags[0]) {
                callFunction(std_Array_shift, subtags);
                i--;
            }
        }
        subtags[i] = subtag;
        i++;
    }

    // Directly return when the language tag doesn't contain any extension or
    // private use sub-tags.
    if (i === subtags.length)
        return callFunction(std_Array_join, subtags, "-");

    var normal = ArrayJoinRange(subtags, "-", 0, i);

    // Extension sequences are sorted by their singleton characters.
    // "u-ca-chinese-t-zh-latn" -> "t-zh-latn-u-ca-chinese"
    var extensions = [];
    while (i < subtags.length && subtags[i] !== "x") {
        var extensionStart = i;
        i++;
        while (i < subtags.length && subtags[i].length > 1)
            i++;
        var extension = ArrayJoinRange(subtags, "-", extensionStart, i);
        _DefineDataProperty(extensions, extensions.length, extension);
    }
    callFunction(ArraySort, extensions);

    // Private use sequences are left as is. "x-private"
    var privateUse = "";
    if (i < subtags.length)
        privateUse = ArrayJoinRange(subtags, "-", i);

    // Put everything back together.
    var canonical = normal;
    if (extensions.length > 0)
        canonical += "-" + callFunction(std_Array_join, extensions, "-");
    if (privateUse.length > 0) {
        // Be careful of a Language-Tag that is entirely privateuse.
        if (canonical.length > 0)
            canonical += "-" + privateUse;
        else
            canonical = privateUse;
    }

    return canonical;
}


/**
 * Joins the array elements in the given range with the supplied separator.
 */
function ArrayJoinRange(array, separator, from, to = array.length) {
    assert(typeof separator === "string", "|separator| is a string value");
    assert(typeof from === "number", "|from| is a number value");
    assert(typeof to === "number", "|to| is a number value");
    assert(0 <= from && from <= to && to <= array.length, "|from| and |to| form a valid range");

    if (from === to)
        return "";

    var result = array[from];
    for (var i = from + 1; i < to; i++) {
        result += separator + array[i];
    }
    return result;
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
 * Validates and canonicalizes the given language tag.
 */
function ValidateAndCanonicalizeLanguageTag(locale) {
    assert(typeof locale === "string", "ValidateAndCanonicalizeLanguageTag");

    // Handle the common case (a standalone language) first.
    // Only the following BCP47 subset is accepted:
    //   Language-Tag  = langtag
    //   langtag       = language
    //   language      = 2*3ALPHA ; shortest ISO 639 code
    // For three character long strings we need to make sure it's not a
    // private use only language tag, for example "x-x".
    if (locale.length === 2 || (locale.length === 3 && locale[1] !== "-")) {
        if (!IsASCIIAlphaString(locale))
            ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, locale);
        assert(IsStructurallyValidLanguageTag(locale), "2*3ALPHA is a valid language tag");

        // The language subtag is canonicalized to lower case.
        locale = callFunction(std_String_toLowerCase, locale);

        // langTagMappings doesn't contain any 2*3ALPHA keys, so we don't need
        // to check for possible replacements in this map.
        assert(!hasOwn(locale, langTagMappings), "langTagMappings contains no 2*3ALPHA mappings");

        // Replace deprecated subtags with their preferred values.
        locale = hasOwn(locale, langSubtagMappings)
                 ? langSubtagMappings[locale]
                 : locale;
        assert(locale === CanonicalizeLanguageTag(locale), "expected same canonicalization");

        return locale;
    }

    if (!IsStructurallyValidLanguageTag(locale))
        ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, locale);

    return CanonicalizeLanguageTag(locale);
}


function localeContainsNoUnicodeExtensions(locale) {
    // No "-u-", no possible Unicode extension.
    if (callFunction(std_String_indexOf, locale, "-u-") === -1)
        return true;

    // "-u-" within privateuse also isn't one.
    if (callFunction(std_String_indexOf, locale, "-u-") > callFunction(std_String_indexOf, locale, "-x-"))
        return true;

    // An entirely-privateuse tag doesn't contain extensions.
    if (callFunction(std_String_startsWith, locale, "x-"))
        return true;

    // Otherwise, we have a Unicode extension sequence.
    return false;
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
    var candidate;
    if (!IsStructurallyValidLanguageTag(runtimeDefaultLocale)) {
        candidate = lastDitchLocale();
    } else {
        candidate = CanonicalizeLanguageTag(runtimeDefaultLocale);

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

    assert(IsStructurallyValidLanguageTag(candidate),
           "the candidate must be structurally valid");
    assert(localeContainsNoUnicodeExtensions(candidate),
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

    assert(IsStructurallyValidLanguageTag(locale),
           "the computed default locale must be structurally valid");
    assert(locale === CanonicalizeLanguageTag(locale),
           "the computed default locale must be canonical");
    assert(localeContainsNoUnicodeExtensions(locale),
           "the computed default locale must not contain a Unicode extension sequence");

    localeCache.defaultLocale = locale;
    localeCache.runtimeDefaultLocale = runtimeDefaultLocale;

    return locale;
}


/**
 * Verifies that the given string is a well-formed ISO 4217 currency code.
 *
 * Spec: ECMAScript Internationalization API Specification, 6.3.1.
 */
function getIsWellFormedCurrencyCodeRE() {
    return internalIntlRegExps.isWellFormedCurrencyCodeRE ||
           (internalIntlRegExps.isWellFormedCurrencyCodeRE = RegExpCreate("[^A-Z]"));
}
function IsWellFormedCurrencyCode(currency) {
    var c = ToString(currency);
    var normalized = toASCIIUpperCase(c);
    if (normalized.length !== 3)
        return false;
    return !regexp_test_no_statics(getIsWellFormedCurrencyCodeRE(), normalized);
}


var timeZoneCache = {
    icuDefaultTimeZone: undefined,
    defaultTimeZone: undefined,
};


/**
 * 6.4.2 CanonicalizeTimeZoneName ( timeZone )
 *
 * Canonicalizes the given IANA time zone name.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 */
function CanonicalizeTimeZoneName(timeZone) {
    assert(typeof timeZone === "string", "CanonicalizeTimeZoneName");

    // Step 1. (Not applicable, the input is already a valid IANA time zone.)
    assert(timeZone !== "Etc/Unknown", "Invalid time zone");
    assert(timeZone === intl_IsValidTimeZoneName(timeZone), "Time zone name not normalized");

    // Step 2.
    var ianaTimeZone = intl_canonicalizeTimeZone(timeZone);
    assert(ianaTimeZone !== "Etc/Unknown", "Invalid canonical time zone");
    assert(ianaTimeZone === intl_IsValidTimeZoneName(ianaTimeZone), "Unsupported canonical time zone");

    // Step 3.
    if (ianaTimeZone === "Etc/UTC" || ianaTimeZone === "Etc/GMT") {
        // ICU/CLDR canonicalizes Etc/UCT to Etc/GMT, but following IANA and
        // ECMA-402 to the letter means Etc/UCT is a separate time zone.
        if (timeZone === "Etc/UCT" || timeZone === "UCT")
            ianaTimeZone = "Etc/UCT";
        else
            ianaTimeZone = "UTC";
    }

    // Step 4.
    return ianaTimeZone;
}


/**
 * 6.4.3 DefaultTimeZone ()
 *
 * Returns the IANA time zone name for the host environment's current time zone.
 *
 * ES2017 Intl draft rev 4a23f407336d382ed5e3471200c690c9b020b5f3
 */
function DefaultTimeZone() {
    if (intl_isDefaultTimeZone(timeZoneCache.icuDefaultTimeZone))
        return timeZoneCache.defaultTimeZone;

    // Verify that the current ICU time zone is a valid ECMA-402 time zone.
    var icuDefaultTimeZone = intl_defaultTimeZone();
    var timeZone = intl_IsValidTimeZoneName(icuDefaultTimeZone);
    if (timeZone === null) {
        // Before defaulting to "UTC", try to represent the default time zone
        // using the Etc/GMT + offset format. This format only accepts full
        // hour offsets.
        const msPerHour = 60 * 60 * 1000;
        var offset = intl_defaultTimeZoneOffset();
        assert(offset === (offset | 0),
               "milliseconds offset shouldn't be able to exceed int32_t range");
        var offsetHours = offset / msPerHour, offsetHoursFraction = offset % msPerHour;
        if (offsetHoursFraction === 0) {
            // Etc/GMT + offset uses POSIX-style signs, i.e. a positive offset
            // means a location west of GMT.
            timeZone = "Etc/GMT" + (offsetHours < 0 ? "+" : "-") + std_Math_abs(offsetHours);

            // Check if the fallback is valid.
            timeZone = intl_IsValidTimeZoneName(timeZone);
        }

        // Fallback to "UTC" if everything else fails.
        if (timeZone === null)
            timeZone = "UTC";
    }

    // Canonicalize the ICU time zone, e.g. change Etc/UTC to UTC.
    var defaultTimeZone = CanonicalizeTimeZoneName(timeZone);

    timeZoneCache.defaultTimeZone = defaultTimeZone;
    timeZoneCache.icuDefaultTimeZone = icuDefaultTimeZone;

    return defaultTimeZone;
}


/********** Locale and Parameter Negotiation **********/

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
    if (locales === undefined)
        return [];
    if (typeof locales === "string") {
        if (!IsStructurallyValidLanguageTag(locales))
            ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, locales);
        return [CanonicalizeLanguageTag(locales)];
    }
    var seen = [];
    var O = ToObject(locales);
    var len = ToLength(O.length);
    var k = 0;
    while (k < len) {
        // Don't call ToString(k) - SpiderMonkey is faster with integers.
        var kPresent = HasProperty(O, k);
        if (kPresent) {
            var kValue = O[k];
            if (!(typeof kValue === "string" || IsObject(kValue)))
                ThrowTypeError(JSMSG_INVALID_LOCALES_ELEMENT);
            var tag = ToString(kValue);
            if (!IsStructurallyValidLanguageTag(tag))
                ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, tag);
            tag = CanonicalizeLanguageTag(tag);
            if (callFunction(ArrayIndexOf, seen, tag) === -1)
                _DefineDataProperty(seen, seen.length, tag);
        }
        k++;
    }
    return seen;
}


function BestAvailableLocaleHelper(availableLocales, locale, considerDefaultLocale) {
    assert(IsStructurallyValidLanguageTag(locale), "invalid BestAvailableLocale locale structure");
    assert(locale === CanonicalizeLanguageTag(locale), "non-canonical BestAvailableLocale locale");
    assert(localeContainsNoUnicodeExtensions(locale), "locale must contain no Unicode extensions");

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
    var i = 0;
    var len = requestedLocales.length;
    var availableLocale;
    var locale, noExtensionsLocale;
    while (i < len && availableLocale === undefined) {
        locale = requestedLocales[i];
        noExtensionsLocale = removeUnicodeExtensions(locale);
        availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);
        i++;
    }

    var result = new Record();
    if (availableLocale !== undefined) {
        result.locale = availableLocale;
        if (locale !== noExtensionsLocale) {
            var unicodeLocaleExtensionSequenceRE = getUnicodeLocaleExtensionSequenceRE();
            var extensionMatch = regexp_exec_no_statics(unicodeLocaleExtensionSequenceRE, locale);
            result.extension = extensionMatch[0];
        }
    } else {
        result.locale = DefaultLocale();
    }
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
 * NOTE: PR to add UnicodeExtensionValue to ECMA-402 isn't yet written.
 */
function UnicodeExtensionValue(extension, key) {
    assert(typeof extension === "string", "extension is a string value");
    assert(function() {
        var unicodeLocaleExtensionSequenceRE = getUnicodeLocaleExtensionSequenceRE();
        var extensionMatch = regexp_exec_no_statics(unicodeLocaleExtensionSequenceRE, extension);
        return extensionMatch !== null && extensionMatch[0] === extension;
    }(), "extension is a Unicode extension subtag");
    assert(typeof key === "string", "key is a string value");
    assert(key.length === 2, "key is a Unicode extension key subtag");

    // Step 1.
    var size = extension.length;

    // Step 2.
    var searchValue = "-" + key + "-";

    // Step 3.
    var pos = callFunction(std_String_indexOf, extension, searchValue);

    // Step 4.
    if (pos !== -1) {
        // Step 4.a.
        var start = pos + 4;

        // Step 4.b.
        var end = start;

        // Step 4.c.
        var k = start;

        // Steps 4.d-e.
        while (true) {
            // Step 4.e.i.
            var e = callFunction(std_String_indexOf, extension, "-", k);

            // Step 4.e.ii.
            var len = e === -1 ? size - k : e - k;

            // Step 4.e.iii.
            if (len === 2)
                break;

            // Step 4.e.iv.
            if (e === -1) {
                end = size;
                break;
            }

            // Step 4.e.v.
            end = e;
            k = e + 1;
        }

        // Step 4.f.
        return callFunction(String_substring, extension, start, end);
    }

    // Step 5.
    searchValue = "-" + key;

    // Steps 6-7.
    if (callFunction(std_String_endsWith, extension, searchValue))
        return "";

    // Step 8 (implicit).
}


/**
 * Compares a BCP 47 language priority list against availableLocales and
 * determines the best available language to meet the request. Options specified
 * through Unicode extension subsequences are negotiated separately, taking the
 * caller's relevant extensions and locale data as well as client-provided
 * options into consideration.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.5.
 */
function ResolveLocale(availableLocales, requestedLocales, options, relevantExtensionKeys, localeData) {
    /*jshint laxbreak: true */

    // Steps 1-3.
    var matcher = options.localeMatcher;
    var r = (matcher === "lookup")
            ? LookupMatcher(availableLocales, requestedLocales)
            : BestFitMatcher(availableLocales, requestedLocales);

    // Step 4.
    var foundLocale = r.locale;

    // Step 5 (Not applicable in this implementation).
    var extension = r.extension;

    // Steps 6-7.
    var result = new Record();
    result.dataLocale = foundLocale;

    // Step 8.
    var supportedExtension = "-u";

    // In this implementation, localeData is a function, not an object.
    var localeDataProvider = localeData();

    // Steps 9-12.
    for (var i = 0; i < relevantExtensionKeys.length; i++) {
        // Step 12.a.
        var key = relevantExtensionKeys[i];

        // Steps 12.b-d (The locale data is only computed when needed).
        var keyLocaleData = undefined;
        var value = undefined;

        // Locale tag may override.

        // Step 12.e.
        var supportedExtensionAddition = "";

        // Step 12.f.
        if (extension !== undefined) {
            // NB: The step annotations don't yet match the ES2017 Intl draft,
            // 94045d234762ad107a3d09bb6f7381a65f1a2f9b, because the PR to add
            // the new UnicodeExtensionValue abstract operation still needs to
            // be written.

            // Step 12.f.i.
            var requestedValue = UnicodeExtensionValue(extension, key);

            // Step 12.f.ii.
            if (requestedValue !== undefined) {
                // Steps 12.b-c.
                keyLocaleData = callFunction(localeDataProvider[key], null, foundLocale);

                // Step 12.f.ii.1.
                if (requestedValue !== "") {
                    // Step 12.f.ii.1.a.
                    if (callFunction(ArrayIndexOf, keyLocaleData, requestedValue) !== -1) {
                        value = requestedValue;
                        supportedExtensionAddition = "-" + key + "-" + value;
                    }
                } else {
                    // Step 12.f.ii.2.

                    // According to the LDML spec, if there's no type value,
                    // and true is an allowed value, it's used.
                    if (callFunction(ArrayIndexOf, keyLocaleData, "true") !== -1)
                        value = "true";
                }
            }
        }

        // Options override all.

        // Step 12.g.i.
        var optionsValue = options[key];

        // Step 12.g, 12.g.ii.
        if (optionsValue !== undefined && optionsValue !== value) {
            // Steps 12.b-c.
            if (keyLocaleData === undefined)
                keyLocaleData = callFunction(localeDataProvider[key], null, foundLocale);

            if (callFunction(ArrayIndexOf, keyLocaleData, optionsValue) !== -1) {
                value = optionsValue;
                supportedExtensionAddition = "";
            }
        }

        // Locale data provides default value.
        if (value === undefined) {
            // Steps 12.b-d.
            value = keyLocaleData === undefined
                    ? callFunction(localeDataProvider.default[key], null, foundLocale)
                    : keyLocaleData[0];
        }

        // Steps 12.h-j.
        assert(typeof value === "string" || value === null, "unexpected locale data value");
        result[key] = value;
        supportedExtension += supportedExtensionAddition;
    }

    // Step 13.
    if (supportedExtension.length > 2) {
        assert(!callFunction(std_String_startsWith, foundLocale, "x-"),
               "unexpected privateuse-only locale returned from ICU");

        // Step 13.a.
        var privateIndex = callFunction(std_String_indexOf, foundLocale, "-x-");

        // Steps 13.b-c.
        if (privateIndex === -1) {
            foundLocale += supportedExtension;
        } else {
            var preExtension = callFunction(String_substring, foundLocale, 0, privateIndex);
            var postExtension = callFunction(String_substring, foundLocale, privateIndex);
            foundLocale = preExtension + supportedExtension + postExtension;
        }

        // Step 13.d.
        assert(IsStructurallyValidLanguageTag(foundLocale), "invalid locale after concatenation");

        // Step 13.e (Not required in this implementation, because we don't
        // canonicalize Unicode extension subtags).
        assert(foundLocale === CanonicalizeLanguageTag(foundLocale), "same locale with extension");
    }

    // Step 14.
    result.locale = foundLocale;

    // Step 15.
    return result;
}


/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.6.
 */
function LookupSupportedLocales(availableLocales, requestedLocales) {
    // Steps 1-2.
    var len = requestedLocales.length;
    var subset = [];

    // Steps 3-4.
    var k = 0;
    while (k < len) {
        // Steps 4.a-b.
        var locale = requestedLocales[k];
        var noExtensionsLocale = removeUnicodeExtensions(locale);

        // Step 4.c-d.
        var availableLocale = BestAvailableLocale(availableLocales, noExtensionsLocale);
        if (availableLocale !== undefined)
            _DefineDataProperty(subset, subset.length, locale);

        // Step 4.e.
        k++;
    }

    // Steps 5-6.
    return subset;
}


/**
 * Returns the subset of requestedLocales for which availableLocales has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.7.
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
 * Spec: ECMAScript Internationalization API Specification, 9.2.8.
 */
function SupportedLocales(availableLocales, requestedLocales, options) {
    /*jshint laxbreak: true */

    // Step 1.
    var matcher;
    if (options !== undefined) {
        // Steps 1.a-b.
        options = ToObject(options);
        matcher = options.localeMatcher;

        // Step 1.c.
        if (matcher !== undefined) {
            matcher = ToString(matcher);
            if (matcher !== "lookup" && matcher !== "best fit")
                ThrowRangeError(JSMSG_INVALID_LOCALE_MATCHER, matcher);
        }
    }

    // Steps 2-3.
    var subset = (matcher === undefined || matcher === "best fit")
                 ? BestFitSupportedLocales(availableLocales, requestedLocales)
                 : LookupSupportedLocales(availableLocales, requestedLocales);

    // Step 4.
    for (var i = 0; i < subset.length; i++) {
        _DefineDataProperty(subset, i, subset[i],
                            ATTR_ENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);
    }
    _DefineDataProperty(subset, "length", subset.length,
                        ATTR_NONENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);

    // Step 5.
    return subset;
}


/**
 * Extracts a property value from the provided options object, converts it to
 * the required type, checks whether it is one of a list of allowed values,
 * and fills in a fallback value if necessary.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.2.9.
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
    // Steps 1-3.
    return DefaultNumberOption(options[property], minimum, maximum, fallback);
}


/********** Property access for Intl objects **********/


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
    assert((type === "Collator" && IsCollator(obj)) ||
           (type === "DateTimeFormat" && IsDateTimeFormat(obj)) ||
           (type === "NumberFormat" && IsNumberFormat(obj)) ||
           (type === "PluralRules" && IsPluralRules(obj)) ||
           (type === "RelativeTimeFormat" && IsRelativeTimeFormat(obj)),
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

    assert(UnsafeGetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT) === null,
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
    assert(IsCollator(obj) || IsDateTimeFormat(obj) ||
           IsNumberFormat(obj) || IsPluralRules(obj) ||
           IsRelativeTimeFormat(obj),
           "getIntlObjectInternals called with non-Intl object");

    var internals = UnsafeGetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT);

    assert(IsObject(internals), "internals not an object");
    assert(hasOwn("type", internals), "missing type");
    assert((internals.type === "Collator" && IsCollator(obj)) ||
           (internals.type === "DateTimeFormat" && IsDateTimeFormat(obj)) ||
           (internals.type === "NumberFormat" && IsNumberFormat(obj)) ||
           (internals.type === "PluralRules" && IsPluralRules(obj)) ||
           (internals.type === "RelativeTimeFormat" && IsRelativeTimeFormat(obj)),
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


/********** Intl.Collator **********/


/**
 * Mapping from Unicode extension keys for collation to options properties,
 * their types and permissible values.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.1.1.
 */
var collatorKeyMappings = {
    kn: {property: "numeric", type: "boolean"},
    kf: {property: "caseFirst", type: "string", values: ["upper", "lower", "false"]}
};


/**
 * Compute an internal properties object from |lazyCollatorData|.
 */
function resolveCollatorInternals(lazyCollatorData) {
    assert(IsObject(lazyCollatorData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    // Step 7.
    internalProps.usage = lazyCollatorData.usage;

    // Step 8.
    var Collator = collatorInternalProperties;

    // Step 9.
    var collatorIsSorting = lazyCollatorData.usage === "sort";
    var localeData = collatorIsSorting
                     ? Collator.sortLocaleData
                     : Collator.searchLocaleData;

    // Compute effective locale.
    // Step 14.
    var relevantExtensionKeys = Collator.relevantExtensionKeys;

    // Step 15.
    var r = ResolveLocale(callFunction(Collator.availableLocales, Collator),
                          lazyCollatorData.requestedLocales,
                          lazyCollatorData.opt,
                          relevantExtensionKeys,
                          localeData);

    // Step 16.
    internalProps.locale = r.locale;

    // Steps 17-19.
    var key, property, value, mapping;
    var i = 0, len = relevantExtensionKeys.length;
    while (i < len) {
        // Step 19.a.
        key = relevantExtensionKeys[i];
        if (key === "co") {
            // Step 19.b.
            property = "collation";
            value = r.co === null ? "default" : r.co;
        } else {
            // Step 19.c.
            mapping = collatorKeyMappings[key];
            property = mapping.property;
            value = r[key];
            if (mapping.type === "boolean")
                value = value === "true";
        }

        // Step 19.d.
        internalProps[property] = value;

        // Step 19.e.
        i++;
    }

    // Compute remaining collation options.
    // Steps 21-22.
    var s = lazyCollatorData.rawSensitivity;
    if (s === undefined) {
        // In theory the default sensitivity for the "search" collator is
        // locale dependent; in reality the CLDR/ICU default strength is
        // always tertiary. Therefore use "variant" as the default value for
        // both collation modes.
        s = "variant";
    }
    internalProps.sensitivity = s;

    // Step 24.
    internalProps.ignorePunctuation = lazyCollatorData.ignorePunctuation;

    // Step 25.
    internalProps.boundFormat = undefined;

    // The caller is responsible for associating |internalProps| with the right
    // object using |setInternalProperties|.
    return internalProps;
}


/**
 * Returns an object containing the Collator internal properties of |obj|.
 */
function getCollatorInternals(obj) {
    assert(IsObject(obj), "getCollatorInternals called with non-object");
    assert(IsCollator(obj), "getCollatorInternals called with non-Collator");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "Collator", "bad type escaped getIntlObjectInternals");

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    internalProps = resolveCollatorInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}


/**
 * Initializes an object as a Collator.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a Collator.  This
 * later work occurs in |resolveCollatorInternals|; steps not noted here occur
 * there.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.1.1.
 */
function InitializeCollator(collator, locales, options) {
    assert(IsObject(collator), "InitializeCollator called with non-object");
    assert(IsCollator(collator), "InitializeCollator called with non-Collator");

    // Steps 1-2 (These steps are no longer required and should be removed
    // from the spec; https://github.com/tc39/ecma402/issues/115).

    // Lazy Collator data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //     usage: "sort" / "search",
    //     opt: // opt object computed in InitializeCollator
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //         kn: true / false / undefined,
    //         kf: "upper" / "lower" / "false" / undefined
    //       }
    //     rawSensitivity: "base" / "accent" / "case" / "variant" / undefined,
    //     ignorePunctuation: true / false
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every Collator lazy data object has *all* these properties, never a
    // subset of them.
    var lazyCollatorData = std_Object_create(null);

    // Step 3.
    var requestedLocales = CanonicalizeLocaleList(locales);
    lazyCollatorData.requestedLocales = requestedLocales;

    // Steps 4-5.
    //
    // If we ever need more speed here at startup, we should try to detect the
    // case where |options === undefined| and then directly use the default
    // value for each option.  For now, just keep it simple.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Compute options that impact interpretation of locale.
    // Step 6.
    var u = GetOption(options, "usage", "string", ["sort", "search"], "sort");
    lazyCollatorData.usage = u;

    // Step 10.
    var opt = new Record();
    lazyCollatorData.opt = opt;

    // Steps 11-12.
    var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    // Step 13, unrolled.
    var numericValue = GetOption(options, "numeric", "boolean", undefined, undefined);
    if (numericValue !== undefined)
        numericValue = numericValue ? "true" : "false";
    opt.kn = numericValue;

    var caseFirstValue = GetOption(options, "caseFirst", "string", ["upper", "lower", "false"], undefined);
    opt.kf = caseFirstValue;

    // Compute remaining collation options.
    // Step 20.
    var s = GetOption(options, "sensitivity", "string",
                      ["base", "accent", "case", "variant"], undefined);
    lazyCollatorData.rawSensitivity = s;

    // Step 23.
    var ip = GetOption(options, "ignorePunctuation", "boolean", undefined, false);
    lazyCollatorData.ignorePunctuation = ip;

    // Step 26.
    //
    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(collator, "Collator", lazyCollatorData);
}


/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.2.2.
 */
function Intl_Collator_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    var availableLocales = callFunction(collatorInternalProperties.availableLocales,
                                        collatorInternalProperties);
    var requestedLocales = CanonicalizeLocaleList(locales);
    return SupportedLocales(availableLocales, requestedLocales, options);
}


/**
 * Collator internal properties.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.1 and 10.2.3.
 */
var collatorInternalProperties = {
    sortLocaleData: collatorSortLocaleData,
    searchLocaleData: collatorSearchLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_Collator_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: ["co", "kn", "kf"]
};


/**
 * Returns the actual locale used when a collator for |locale| is constructed.
 */
function collatorActualLocale(locale) {
    assert(typeof locale === "string", "locale should be string");

    // If |locale| is the default locale (e.g. da-DK), but only supported
    // through a fallback (da), we need to get the actual locale before we
    // can call intl_isUpperCaseFirst. Also see BestAvailableLocaleHelper.
    var availableLocales = callFunction(collatorInternalProperties.availableLocales,
                                        collatorInternalProperties);
    return BestAvailableLocaleIgnoringDefault(availableLocales, locale);
}


/**
 * Returns the default caseFirst values for the given locale. The first
 * element in the returned array denotes the default value per ES2017 Intl,
 * 9.1 Internal slots of Service Constructors.
 */
function collatorSortCaseFirst(locale) {
    var actualLocale = collatorActualLocale(locale);
    if (intl_isUpperCaseFirst(actualLocale))
        return ["upper", "false", "lower"];

    // Default caseFirst values for all other languages.
    return ["false", "lower", "upper"];
}


/**
 * Returns the default caseFirst value for the given locale.
 */
function collatorSortCaseFirstDefault(locale) {
    var actualLocale = collatorActualLocale(locale);
    if (intl_isUpperCaseFirst(actualLocale))
        return "upper";

    // Default caseFirst value for all other languages.
    return "false";
}


function collatorSortLocaleData() {
    /* eslint-disable object-shorthand */
    return {
        co: intl_availableCollations,
        kn: function() {
            return ["false", "true"];
        },
        kf: collatorSortCaseFirst,
        default: {
            co: function() {
                // The first element of the collations array must be |null|
                // per ES2017 Intl, 10.2.3 Internal Slots.
                return null;
            },
            kn: function() {
                return "false";
            },
            kf: collatorSortCaseFirstDefault,
        }
    };
    /* eslint-enable object-shorthand */
}


function collatorSearchLocaleData() {
    /* eslint-disable object-shorthand */
    return {
        co: function() {
            return [null];
        },
        kn: function() {
            return ["false", "true"];
        },
        kf: function() {
            return ["false", "lower", "upper"];
        },
        default: {
            co: function() {
                return null;
            },
            kn: function() {
                return "false";
            },
            kf: function() {
                return "false";
            },
        }
    };
    /* eslint-enable object-shorthand */
}


/**
 * Function to be bound and returned by Intl.Collator.prototype.format.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 */
function collatorCompareToBind(x, y) {
    // Step 1.
    var collator = this;

    // Step 2.
    assert(IsObject(collator), "collatorCompareToBind called with non-object");
    assert(IsCollator(collator), "collatorCompareToBind called with non-Collator");

    // Steps 3-6
    var X = ToString(x);
    var Y = ToString(y);

    // Step 7.
    return intl_CompareStrings(collator, X, Y);
}


/**
 * Returns a function bound to this Collator that compares x (converted to a
 * String value) and y (converted to a String value),
 * and returns a number less than 0 if x < y, 0 if x = y, or a number greater
 * than 0 if x > y according to the sort order for the locale and collation
 * options of this Collator object.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.2.
 */
function Intl_Collator_compare_get() {
    // Step 1.
    var collator = this;

    // Steps 2-3.
    if (!IsObject(collator) || !IsCollator(collator))
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "Collator", "compare", "Collator");

    var internals = getCollatorInternals(collator);

    // Step 4.
    if (internals.boundCompare === undefined) {
        // Step 4.a.
        var F = collatorCompareToBind;

        // Steps 4.b-d.
        var bc = callFunction(FunctionBind, F, collator);
        internals.boundCompare = bc;
    }

    // Step 5.
    return internals.boundCompare;
}
_SetCanonicalName(Intl_Collator_compare_get, "get compare");


/**
 * Returns the resolved options for a Collator object.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.3 and 10.4.
 */
function Intl_Collator_resolvedOptions() {
    // Check "this Collator object" per introduction of section 10.3.
    if (!IsObject(this) || !IsCollator(this))
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "Collator", "resolvedOptions", "Collator");

    var internals = getCollatorInternals(this);

    var result = {
        locale: internals.locale,
        usage: internals.usage,
        sensitivity: internals.sensitivity,
        ignorePunctuation: internals.ignorePunctuation
    };

    var relevantExtensionKeys = collatorInternalProperties.relevantExtensionKeys;
    for (var i = 0; i < relevantExtensionKeys.length; i++) {
        var key = relevantExtensionKeys[i];
        var property = (key === "co") ? "collation" : collatorKeyMappings[key].property;
        _DefineDataProperty(result, property, internals[property]);
    }
    return result;
}


/********** Intl.NumberFormat **********/


/**
 * NumberFormat internal properties.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.1 and 11.2.3.
 */
var numberFormatInternalProperties = {
    localeData: numberFormatLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_NumberFormat_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: ["nu"]
};


/**
 * Compute an internal properties object from |lazyNumberFormatData|.
 */
function resolveNumberFormatInternals(lazyNumberFormatData) {
    assert(IsObject(lazyNumberFormatData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    // Step 3.
    var requestedLocales = lazyNumberFormatData.requestedLocales;

    // Compute options that impact interpretation of locale.
    // Step 6.
    var opt = lazyNumberFormatData.opt;

    var NumberFormat = numberFormatInternalProperties;

    // Step 9.
    var localeData = NumberFormat.localeData;

    // Step 10.
    var r = ResolveLocale(callFunction(NumberFormat.availableLocales, NumberFormat),
                          requestedLocales,
                          opt,
                          NumberFormat.relevantExtensionKeys,
                          localeData);

    // Steps 11-12.  (Step 13 is not relevant to our implementation.)
    internalProps.locale = r.locale;
    internalProps.numberingSystem = r.nu;

    // Compute formatting options.
    // Step 15.
    var style = lazyNumberFormatData.style;
    internalProps.style = style;

    // Steps 19, 21.
    if (style === "currency") {
        internalProps.currency = lazyNumberFormatData.currency;
        internalProps.currencyDisplay = lazyNumberFormatData.currencyDisplay;
    }

    internalProps.minimumIntegerDigits = lazyNumberFormatData.minimumIntegerDigits;
    internalProps.minimumFractionDigits = lazyNumberFormatData.minimumFractionDigits;
    internalProps.maximumFractionDigits = lazyNumberFormatData.maximumFractionDigits;

    if ("minimumSignificantDigits" in lazyNumberFormatData) {
        // Note: Intl.NumberFormat.prototype.resolvedOptions() exposes the
        // actual presence (versus undefined-ness) of these properties.
        assert("maximumSignificantDigits" in lazyNumberFormatData, "min/max sig digits mismatch");
        internalProps.minimumSignificantDigits = lazyNumberFormatData.minimumSignificantDigits;
        internalProps.maximumSignificantDigits = lazyNumberFormatData.maximumSignificantDigits;
    }

    // Step 27.
    internalProps.useGrouping = lazyNumberFormatData.useGrouping;

    // Step 34.
    internalProps.boundFormat = undefined;

    // The caller is responsible for associating |internalProps| with the right
    // object using |setInternalProperties|.
    return internalProps;
}


/**
 * Returns an object containing the NumberFormat internal properties of |obj|.
 */
function getNumberFormatInternals(obj) {
    assert(IsObject(obj), "getNumberFormatInternals called with non-object");
    assert(IsNumberFormat(obj), "getNumberFormatInternals called with non-NumberFormat");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "NumberFormat", "bad type escaped getIntlObjectInternals");

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    internalProps = resolveNumberFormatInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}


/**
 * UnwrapNumberFormat(nf)
 */
function UnwrapNumberFormat(nf, methodName) {
    // Step 1.
    if (IsObject(nf) && !IsNumberFormat(nf) && nf instanceof GetNumberFormatConstructor())
        nf = nf[intlFallbackSymbol()];

    // Step 2.
    if (!IsObject(nf) || !IsNumberFormat(nf))
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "NumberFormat", methodName, "NumberFormat");

    // Step 3.
    return nf;
}


/**
 * Applies digit options used for number formatting onto the intl object.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.1.1.
 */
function SetNumberFormatDigitOptions(lazyData, options, mnfdDefault, mxfdDefault) {
    // We skip Step 1 because we set the properties on a lazyData object.

    // Step 2-3.
    assert(IsObject(options), "SetNumberFormatDigitOptions");
    assert(typeof mnfdDefault === "number", "SetNumberFormatDigitOptions");
    assert(typeof mxfdDefault === "number", "SetNumberFormatDigitOptions");
    assert(mnfdDefault <= mxfdDefault, "SetNumberFormatDigitOptions");

    // Steps 4-8.
    const mnid = GetNumberOption(options, "minimumIntegerDigits", 1, 21, 1);
    const mnfd = GetNumberOption(options, "minimumFractionDigits", 0, 20, mnfdDefault);
    const mxfdActualDefault = std_Math_max(mnfd, mxfdDefault);
    const mxfd = GetNumberOption(options, "maximumFractionDigits", mnfd, 20, mxfdActualDefault);

    // Steps 9-10.
    let mnsd = options.minimumSignificantDigits;
    let mxsd = options.maximumSignificantDigits;

    // Steps 11-13.
    lazyData.minimumIntegerDigits = mnid;
    lazyData.minimumFractionDigits = mnfd;
    lazyData.maximumFractionDigits = mxfd;

    // Step 14.
    if (mnsd !== undefined || mxsd !== undefined) {
        mnsd = DefaultNumberOption(mnsd, 1, 21, 1);
        mxsd = DefaultNumberOption(mxsd, mnsd, 21, 21);
        lazyData.minimumSignificantDigits = mnsd;
        lazyData.maximumSignificantDigits = mxsd;
    }
}


/**
 * Initializes an object as a NumberFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a NumberFormat.
 * This later work occurs in |resolveNumberFormatInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.1.1.
 */
function InitializeNumberFormat(numberFormat, thisValue, locales, options) {
    assert(IsObject(numberFormat), "InitializeNumberFormat called with non-object");
    assert(IsNumberFormat(numberFormat), "InitializeNumberFormat called with non-NumberFormat");

    // Steps 1-2 (These steps are no longer required and should be removed
    // from the spec; https://github.com/tc39/ecma402/issues/115).

    // Lazy NumberFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //     style: "decimal" / "percent" / "currency",
    //
    //     // fields present only if style === "currency":
    //     currency: a well-formed currency code (IsWellFormedCurrencyCode),
    //     currencyDisplay: "code" / "symbol" / "name",
    //
    //     opt: // opt object computed in InitializeNumberFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //
    //     minimumIntegerDigits: integer ∈ [1, 21],
    //     minimumFractionDigits: integer ∈ [0, 20],
    //     maximumFractionDigits: integer ∈ [0, 20],
    //
    //     // optional
    //     minimumSignificantDigits: integer ∈ [1, 21],
    //     maximumSignificantDigits: integer ∈ [1, 21],
    //
    //     useGrouping: true / false,
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every NumberFormat lazy data object has *all* these properties, never a
    // subset of them.
    var lazyNumberFormatData = std_Object_create(null);

    // Step 3.
    var requestedLocales = CanonicalizeLocaleList(locales);
    lazyNumberFormatData.requestedLocales = requestedLocales;

    // Steps 4-5.
    //
    // If we ever need more speed here at startup, we should try to detect the
    // case where |options === undefined| and then directly use the default
    // value for each option.  For now, just keep it simple.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Compute options that impact interpretation of locale.
    // Step 6.
    var opt = new Record();
    lazyNumberFormatData.opt = opt;

    // Steps 7-8.
    var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    // Compute formatting options.
    // Step 14.
    var style = GetOption(options, "style", "string", ["decimal", "percent", "currency"], "decimal");
    lazyNumberFormatData.style = style;

    // Steps 16-19.
    var c = GetOption(options, "currency", "string", undefined, undefined);
    if (c !== undefined && !IsWellFormedCurrencyCode(c))
        ThrowRangeError(JSMSG_INVALID_CURRENCY_CODE, c);
    var cDigits;
    if (style === "currency") {
        if (c === undefined)
            ThrowTypeError(JSMSG_UNDEFINED_CURRENCY);

        // Steps 19.a-c.
        c = toASCIIUpperCase(c);
        lazyNumberFormatData.currency = c;
        cDigits = CurrencyDigits(c);
    }

    // Step 20.
    var cd = GetOption(options, "currencyDisplay", "string", ["code", "symbol", "name"], "symbol");
    if (style === "currency")
        lazyNumberFormatData.currencyDisplay = cd;

    // Steps 22-25.
    var mnfdDefault, mxfdDefault;
    if (style === "currency") {
        mnfdDefault = cDigits;
        mxfdDefault = cDigits;
    } else {
        mnfdDefault = 0;
        mxfdDefault = style === "percent" ? 0 : 3;
    }
    SetNumberFormatDigitOptions(lazyNumberFormatData, options, mnfdDefault, mxfdDefault);

    // Steps 26.
    var g = GetOption(options, "useGrouping", "boolean", undefined, true);
    lazyNumberFormatData.useGrouping = g;

    // Steps 35-36.
    //
    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(numberFormat, "NumberFormat", lazyNumberFormatData);

    if (numberFormat !== thisValue && IsObject(thisValue) &&
        thisValue instanceof GetNumberFormatConstructor())
    {
        _DefineDataProperty(thisValue, intlFallbackSymbol(), numberFormat,
                            ATTR_NONENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);

        return thisValue;
    }

    return numberFormat;
}


/**
 * Returns the number of decimal digits to be used for the given currency.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.1.1.
 */
function getCurrencyDigitsRE() {
    return internalIntlRegExps.currencyDigitsRE ||
           (internalIntlRegExps.currencyDigitsRE = RegExpCreate("^[A-Z]{3}$"));
}
function CurrencyDigits(currency) {
    assert(typeof currency === "string", "CurrencyDigits");
    assert(regexp_test_no_statics(getCurrencyDigitsRE(), currency), "CurrencyDigits");

    if (hasOwn(currency, currencyDigits))
        return currencyDigits[currency];
    return 2;
}


/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.2.2.
 */
function Intl_NumberFormat_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    var availableLocales = callFunction(numberFormatInternalProperties.availableLocales,
                                        numberFormatInternalProperties);
    var requestedLocales = CanonicalizeLocaleList(locales);
    return SupportedLocales(availableLocales, requestedLocales, options);
}


function getNumberingSystems(locale) {
    // ICU doesn't have an API to determine the set of numbering systems
    // supported for a locale; it generally pretends that any numbering system
    // can be used with any locale. Supporting a decimal numbering system
    // (where only the digits are replaced) is easy, so we offer them all here.
    // Algorithmic numbering systems are typically tied to one locale, so for
    // lack of information we don't offer them. To increase chances that
    // other software will process output correctly, we further restrict to
    // those decimal numbering systems explicitly listed in table 2 of
    // the ECMAScript Internationalization API Specification, 11.3.2, which
    // in turn are those with full specifications in version 21 of Unicode
    // Technical Standard #35 using digits that were defined in Unicode 5.0,
    // the Unicode version supported in Windows Vista.
    // The one thing we can find out from ICU is the default numbering system
    // for a locale.
    var defaultNumberingSystem = intl_numberingSystem(locale);
    return [
        defaultNumberingSystem,
        "arab", "arabext", "bali", "beng", "deva",
        "fullwide", "gujr", "guru", "hanidec", "khmr",
        "knda", "laoo", "latn", "limb", "mlym",
        "mong", "mymr", "orya", "tamldec", "telu",
        "thai", "tibt"
    ];
}


function numberFormatLocaleData() {
    return {
        nu: getNumberingSystems,
        default: {
            nu: intl_numberingSystem,
        }
    };
}


/**
 * Function to be bound and returned by Intl.NumberFormat.prototype.format.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 */
function numberFormatFormatToBind(value) {
    // Step 1.
    var nf = this;

    // Step 2.
    assert(IsObject(nf), "InitializeNumberFormat called with non-object");
    assert(IsNumberFormat(nf), "InitializeNumberFormat called with non-NumberFormat");

    // Steps 3-4.
    var x = ToNumber(value);

    // Step 5.
    return intl_FormatNumber(nf, x, /* formatToParts = */ false);
}


/**
 * Returns a function bound to this NumberFormat that returns a String value
 * representing the result of calling ToNumber(value) according to the
 * effective locale and the formatting options of this NumberFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 */
function Intl_NumberFormat_format_get() {
    // Steps 1-3.
    var nf = UnwrapNumberFormat(this, "format");

    var internals = getNumberFormatInternals(nf);

    // Step 4.
    if (internals.boundFormat === undefined) {
        // Step 4.a.
        var F = numberFormatFormatToBind;

        // Steps 4.b-d.
        var bf = callFunction(FunctionBind, F, nf);
        internals.boundFormat = bf;
    }

    // Step 5.
    return internals.boundFormat;
}
_SetCanonicalName(Intl_NumberFormat_format_get, "get format");


function Intl_NumberFormat_formatToParts(value) {
    // Step 1.
    var nf = this;

    // Steps 2-3.
    if (!IsObject(nf) || !IsNumberFormat(nf)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "NumberFormat", "formatToParts",
                       "NumberFormat");
    }

    // Ensure the NumberFormat internals are resolved.
    getNumberFormatInternals(nf);

    // Step 4.
    var x = ToNumber(value);

    // Step 5.
    return intl_FormatNumber(nf, x, /* formatToParts = */ true);
}


/**
 * Returns the resolved options for a NumberFormat object.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.3 and 11.4.
 */
function Intl_NumberFormat_resolvedOptions() {
    // Invoke |UnwrapNumberFormat| per introduction of section 11.3.
    var nf = UnwrapNumberFormat(this, "resolvedOptions");

    var internals = getNumberFormatInternals(nf);

    var result = {
        locale: internals.locale,
        numberingSystem: internals.numberingSystem,
        style: internals.style,
        minimumIntegerDigits: internals.minimumIntegerDigits,
        minimumFractionDigits: internals.minimumFractionDigits,
        maximumFractionDigits: internals.maximumFractionDigits,
        useGrouping: internals.useGrouping
    };
    var optionalProperties = [
        "currency",
        "currencyDisplay",
        "minimumSignificantDigits",
        "maximumSignificantDigits"
    ];
    for (var i = 0; i < optionalProperties.length; i++) {
        var p = optionalProperties[i];
        if (hasOwn(p, internals))
            _DefineDataProperty(result, p, internals[p]);
    }
    return result;
}


/********** Intl.DateTimeFormat **********/


/**
 * Compute an internal properties object from |lazyDateTimeFormatData|.
 */
function resolveDateTimeFormatInternals(lazyDateTimeFormatData) {
    assert(IsObject(lazyDateTimeFormatData), "lazy data not an object?");

    // Lazy DateTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //
    //     localeOpt: // *first* opt computed in InitializeDateTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //
    //         hour12: true / false,  // optional
    //
    //         hourCycle: "h11" / "h12" / "h23" / "h24", // optional
    //       }
    //
    //     timeZone: IANA time zone name,
    //
    //     formatOpt: // *second* opt computed in InitializeDateTimeFormat
    //       {
    //         // all the properties/values listed in Table 3
    //         // (weekday, era, year, month, day, &c.)
    //       }
    //
    //     formatMatcher: "basic" / "best fit",
    //
    //     mozExtensions: true / false,
    //
    //
    //     // If mozExtensions is true:
    //
    //     dateStyle: "full" / "long" / "medium" / "short" / undefined,
    //
    //     timeStyle: "full" / "long" / "medium" / "short" / undefined,
    //
    //     patternOption:
    //       String representing LDML Date Format pattern or undefined
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every DateTimeFormat lazy data object has *all* these properties,
    // never a subset of them.

    var internalProps = std_Object_create(null);

    // Compute effective locale.
    // Step 8.
    var DateTimeFormat = dateTimeFormatInternalProperties;

    // Step 9.
    var localeData = DateTimeFormat.localeData;

    // Step 10.
    var r = ResolveLocale(callFunction(DateTimeFormat.availableLocales, DateTimeFormat),
                          lazyDateTimeFormatData.requestedLocales,
                          lazyDateTimeFormatData.localeOpt,
                          DateTimeFormat.relevantExtensionKeys,
                          localeData);

    // Steps 11-13.
    internalProps.locale = r.locale;
    internalProps.calendar = r.ca;
    internalProps.numberingSystem = r.nu;

    // Compute formatting options.
    // Step 14.
    var dataLocale = r.dataLocale;

    // Steps 15-17.
    internalProps.timeZone = lazyDateTimeFormatData.timeZone;

    // Step 18.
    var formatOpt = lazyDateTimeFormatData.formatOpt;

    // Copy the hourCycle setting, if present, to the format options. But
    // only do this if no hour12 option is present, because the latter takes
    // precedence over hourCycle.
    if (r.hc !== null && formatOpt.hour12 === undefined)
        formatOpt.hourCycle = r.hc;

    // Steps 27-28, more or less - see comment after this function.
    var pattern;
    if (lazyDateTimeFormatData.mozExtensions) {
        if (lazyDateTimeFormatData.patternOption !== undefined) {
            pattern = lazyDateTimeFormatData.patternOption;

            internalProps.patternOption = lazyDateTimeFormatData.patternOption;
        } else if (lazyDateTimeFormatData.dateStyle || lazyDateTimeFormatData.timeStyle) {
            pattern = intl_patternForStyle(dataLocale,
              lazyDateTimeFormatData.dateStyle, lazyDateTimeFormatData.timeStyle,
              lazyDateTimeFormatData.timeZone);

            internalProps.dateStyle = lazyDateTimeFormatData.dateStyle;
            internalProps.timeStyle = lazyDateTimeFormatData.timeStyle;
        } else {
            pattern = toBestICUPattern(dataLocale, formatOpt);
        }
        internalProps.mozExtensions = true;
    } else {
      pattern = toBestICUPattern(dataLocale, formatOpt);
    }

    // If the hourCycle option was set, adjust the resolved pattern to use the
    // requested hour cycle representation.
    if (formatOpt.hourCycle !== undefined)
        pattern = replaceHourRepresentation(pattern, formatOpt.hourCycle);

    // Step 29.
    internalProps.pattern = pattern;

    // Step 30.
    internalProps.boundFormat = undefined;

    // The caller is responsible for associating |internalProps| with the right
    // object using |setInternalProperties|.
    return internalProps;
}


/**
 * Replaces all hour pattern characters in |pattern| to use the matching hour
 * representation for |hourCycle|.
 */
function replaceHourRepresentation(pattern, hourCycle) {
    var hour;
    switch (hourCycle) {
      case "h11":
        hour = "K";
        break;
      case "h12":
        hour = "h";
        break;
      case "h23":
        hour = "H";
        break;
      case "h24":
        hour = "k";
        break;
    }
    assert(hour !== undefined, "Unexpected hourCycle requested: " + hourCycle);

    // Parse the pattern according to the format specified in
    // https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns
    // and replace all hour symbols with |hour|.
    var resultPattern = "";
    var inQuote = false;
    for (var i = 0; i < pattern.length; i++) {
        var ch = pattern[i];
        if (ch === "'") {
            inQuote = !inQuote;
        } else if (!inQuote && (ch === "h" || ch === "H" || ch === "k" || ch === "K")) {
            ch = hour;
        }
        resultPattern += ch;
    }

    return resultPattern;
}


/**
 * Returns an object containing the DateTimeFormat internal properties of |obj|.
 */
function getDateTimeFormatInternals(obj) {
    assert(IsObject(obj), "getDateTimeFormatInternals called with non-object");
    assert(IsDateTimeFormat(obj), "getDateTimeFormatInternals called with non-DateTimeFormat");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "DateTimeFormat", "bad type escaped getIntlObjectInternals");

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    internalProps = resolveDateTimeFormatInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}


/**
 * UnwrapDateTimeFormat(dtf)
 */
function UnwrapDateTimeFormat(dtf, methodName) {
    // Step 1.
    if (IsObject(dtf) && !IsDateTimeFormat(dtf) && dtf instanceof GetDateTimeFormatConstructor())
        dtf = dtf[intlFallbackSymbol()];

    // Step 2.
    if (!IsObject(dtf) || !IsDateTimeFormat(dtf)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "DateTimeFormat", methodName,
                       "DateTimeFormat");
    }

    // Step 3.
    return dtf;
}


/**
 * Initializes an object as a DateTimeFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a DateTimeFormat.
 * This later work occurs in |resolveDateTimeFormatInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function InitializeDateTimeFormat(dateTimeFormat, thisValue, locales, options, mozExtensions) {
    assert(IsObject(dateTimeFormat), "InitializeDateTimeFormat called with non-Object");
    assert(IsDateTimeFormat(dateTimeFormat),
           "InitializeDateTimeFormat called with non-DateTimeFormat");

    // Steps 1-2 (These steps are no longer required and should be removed
    // from the spec; https://github.com/tc39/ecma402/issues/115).

    // Lazy DateTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //
    //     localeOpt: // *first* opt computed in InitializeDateTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //
    //     timeZone: IANA time zone name,
    //
    //     formatOpt: // *second* opt computed in InitializeDateTimeFormat
    //       {
    //         // all the properties/values listed in Table 3
    //         // (weekday, era, year, month, day, &c.)
    //
    //         hour12: true / false,  // optional
    //         hourCycle: "h11" / "h12" / "h23" / "h24", // optional
    //       }
    //
    //     formatMatcher: "basic" / "best fit",
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every DateTimeFormat lazy data object has *all* these properties,
    // never a subset of them.
    var lazyDateTimeFormatData = std_Object_create(null);

    // Step 1.
    var requestedLocales = CanonicalizeLocaleList(locales);
    lazyDateTimeFormatData.requestedLocales = requestedLocales;

    // Step 2.
    options = ToDateTimeOptions(options, "any", "date");

    // Compute options that impact interpretation of locale.
    // Step 3.
    var localeOpt = new Record();
    lazyDateTimeFormatData.localeOpt = localeOpt;

    // Steps 4-5.
    var localeMatcher =
        GetOption(options, "localeMatcher", "string", ["lookup", "best fit"],
                  "best fit");
    localeOpt.localeMatcher = localeMatcher;

    // Step 6.
    var hc = GetOption(options, "hourCycle", "string", ["h11", "h12", "h23", "h24"], undefined);
    localeOpt.hc = hc;

    // Steps 15-18.
    var tz = options.timeZone;
    if (tz !== undefined) {
        // Step 15.a.
        tz = ToString(tz);

        // Step 15.b.
        var timeZone = intl_IsValidTimeZoneName(tz);
        if (timeZone === null)
            ThrowRangeError(JSMSG_INVALID_TIME_ZONE, tz);

        // Step 15.c.
        tz = CanonicalizeTimeZoneName(timeZone);
    } else {
        // Step 16.
        tz = DefaultTimeZone();
    }
    lazyDateTimeFormatData.timeZone = tz;

    // Step 19.
    var formatOpt = new Record();
    lazyDateTimeFormatData.formatOpt = formatOpt;

    lazyDateTimeFormatData.mozExtensions = mozExtensions;

    if (mozExtensions) {
        let pattern = GetOption(options, "pattern", "string", undefined, undefined);
        lazyDateTimeFormatData.patternOption = pattern;

        let dateStyle = GetOption(options, "dateStyle", "string", ["full", "long", "medium", "short"], undefined);
        lazyDateTimeFormatData.dateStyle = dateStyle;
        let timeStyle = GetOption(options, "timeStyle", "string", ["full", "long", "medium", "short"], undefined);
        lazyDateTimeFormatData.timeStyle = timeStyle;
    }

    // Step 20.
    // 12.1, Table 4: Components of date and time formats.
    formatOpt.weekday = GetOption(options, "weekday", "string", ["narrow", "short", "long"],
                                  undefined);
    formatOpt.era = GetOption(options, "era", "string", ["narrow", "short", "long"], undefined);
    formatOpt.year = GetOption(options, "year", "string", ["2-digit", "numeric"], undefined);
    formatOpt.month = GetOption(options, "month", "string",
                                ["2-digit", "numeric", "narrow", "short", "long"], undefined);
    formatOpt.day = GetOption(options, "day", "string", ["2-digit", "numeric"], undefined);
    formatOpt.hour = GetOption(options, "hour", "string", ["2-digit", "numeric"], undefined);
    formatOpt.minute = GetOption(options, "minute", "string", ["2-digit", "numeric"], undefined);
    formatOpt.second = GetOption(options, "second", "string", ["2-digit", "numeric"], undefined);
    formatOpt.timeZoneName = GetOption(options, "timeZoneName", "string", ["short", "long"],
                                       undefined);

    // Steps 21-22 provided by ICU - see comment after this function.

    // Step 23.
    //
    // For some reason (ICU not exposing enough interface?) we drop the
    // requested format matcher on the floor after this.  In any case, even if
    // doing so is justified, we have to do this work here in case it triggers
    // getters or similar. (bug 852837)
    var formatMatcher =
        GetOption(options, "formatMatcher", "string", ["basic", "best fit"],
                  "best fit");
    void formatMatcher;

    // Steps 24-26 provided by ICU, more or less - see comment after this function.

    // Step 27.
    var hr12  = GetOption(options, "hour12", "boolean", undefined, undefined);

    // Pass hr12 on to ICU.
    if (hr12 !== undefined)
        formatOpt.hour12 = hr12;

    // Step 31.
    //
    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(dateTimeFormat, "DateTimeFormat", lazyDateTimeFormatData);

    if (dateTimeFormat !== thisValue && IsObject(thisValue) &&
        thisValue instanceof GetDateTimeFormatConstructor())
    {
        _DefineDataProperty(thisValue, intlFallbackSymbol(), dateTimeFormat,
                            ATTR_NONENUMERABLE | ATTR_NONCONFIGURABLE | ATTR_NONWRITABLE);

        return thisValue;
    }

    return dateTimeFormat;
}


// Intl.DateTimeFormat and ICU skeletons and patterns
// ==================================================
//
// Different locales have different ways to display dates using the same
// basic components. For example, en-US might use "Sept. 24, 2012" while
// fr-FR might use "24 Sept. 2012". The intent of Intl.DateTimeFormat is to
// permit production of a format for the locale that best matches the
// set of date-time components and their desired representation as specified
// by the API client.
//
// ICU supports specification of date and time formats in three ways:
//
// 1) A style is just one of the identifiers FULL, LONG, MEDIUM, or SHORT.
//    The date-time components included in each style and their representation
//    are defined by ICU using CLDR locale data (CLDR is the Unicode
//    Consortium's Common Locale Data Repository).
//
// 2) A skeleton is a string specifying which date-time components to include,
//    and which representations to use for them. For example, "yyyyMMMMdd"
//    specifies a year with at least four digits, a full month name, and a
//    two-digit day. It does not specify in which order the components appear,
//    how they are separated, the localized strings for textual components
//    (such as weekday or month), whether the month is in format or
//    stand-alone form¹, or the numbering system used for numeric components.
//    All that information is filled in by ICU using CLDR locale data.
//    ¹ The format form is the one used in formatted strings that include a
//    day; the stand-alone form is used when not including days, e.g., in
//    calendar headers. The two forms differ at least in some Slavic languages,
//    e.g. Russian: "22 марта 2013 г." vs. "Март 2013".
//
// 3) A pattern is a string specifying which date-time components to include,
//    in which order, with which separators, in which grammatical case. For
//    example, "EEEE, d MMMM y" specifies the full localized weekday name,
//    followed by comma and space, followed by the day, followed by space,
//    followed by the full month name in format form, followed by space,
//    followed by the full year. It
//    still does not specify localized strings for textual components and the
//    numbering system - these are determined by ICU using CLDR locale data or
//    possibly API parameters.
//
// All actual formatting in ICU is done with patterns; styles and skeletons
// have to be mapped to patterns before processing.
//
// The options of DateTimeFormat most closely correspond to ICU skeletons. This
// implementation therefore, in the toBestICUPattern function, converts
// DateTimeFormat options to ICU skeletons, and then lets ICU map skeletons to
// actual ICU patterns. The pattern may not directly correspond to what the
// skeleton requests, as the mapper (UDateTimePatternGenerator) is constrained
// by the available locale data for the locale. The resulting ICU pattern is
// kept as the DateTimeFormat's [[pattern]] internal property and passed to ICU
// in the format method.
//
// An ICU pattern represents the information of the following DateTimeFormat
// internal properties described in the specification, which therefore don't
// exist separately in the implementation:
// - [[weekday]], [[era]], [[year]], [[month]], [[day]], [[hour]], [[minute]],
//   [[second]], [[timeZoneName]]
// - [[hour12]]
// - [[hourCycle]]
// - [[hourNo0]]
// When needed for the resolvedOptions method, the resolveICUPattern function
// maps the instance's ICU pattern back to the specified properties of the
// object returned by resolvedOptions.
//
// ICU date-time skeletons and patterns aren't fully documented in the ICU
// documentation (see http://bugs.icu-project.org/trac/ticket/9627). The best
// documentation at this point is in UTR 35:
// http://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns


/**
 * Returns an ICU pattern string for the given locale and representing the
 * specified options as closely as possible given available locale data.
 */
function toBestICUPattern(locale, options) {
    // Create an ICU skeleton representing the specified options. See
    // http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
    var skeleton = "";
    switch (options.weekday) {
    case "narrow":
        skeleton += "EEEEE";
        break;
    case "short":
        skeleton += "E";
        break;
    case "long":
        skeleton += "EEEE";
    }
    switch (options.era) {
    case "narrow":
        skeleton += "GGGGG";
        break;
    case "short":
        skeleton += "G";
        break;
    case "long":
        skeleton += "GGGG";
        break;
    }
    switch (options.year) {
    case "2-digit":
        skeleton += "yy";
        break;
    case "numeric":
        skeleton += "y";
        break;
    }
    switch (options.month) {
    case "2-digit":
        skeleton += "MM";
        break;
    case "numeric":
        skeleton += "M";
        break;
    case "narrow":
        skeleton += "MMMMM";
        break;
    case "short":
        skeleton += "MMM";
        break;
    case "long":
        skeleton += "MMMM";
        break;
    }
    switch (options.day) {
    case "2-digit":
        skeleton += "dd";
        break;
    case "numeric":
        skeleton += "d";
        break;
    }
    // If hour12 and hourCycle are both present, hour12 takes precedence.
    var hourSkeletonChar = "j";
    if (options.hour12 !== undefined) {
        if (options.hour12)
            hourSkeletonChar = "h";
        else
            hourSkeletonChar = "H";
    } else {
        switch (options.hourCycle) {
        case "h11":
        case "h12":
            hourSkeletonChar = "h";
            break;
        case "h23":
        case "h24":
            hourSkeletonChar = "H";
            break;
        }
    }
    switch (options.hour) {
    case "2-digit":
        skeleton += hourSkeletonChar + hourSkeletonChar;
        break;
    case "numeric":
        skeleton += hourSkeletonChar;
        break;
    }
    switch (options.minute) {
    case "2-digit":
        skeleton += "mm";
        break;
    case "numeric":
        skeleton += "m";
        break;
    }
    switch (options.second) {
    case "2-digit":
        skeleton += "ss";
        break;
    case "numeric":
        skeleton += "s";
        break;
    }
    switch (options.timeZoneName) {
    case "short":
        skeleton += "z";
        break;
    case "long":
        skeleton += "zzzz";
        break;
    }

    // Let ICU convert the ICU skeleton to an ICU pattern for the given locale.
    return intl_patternForSkeleton(locale, skeleton);
}


/**
 * Returns a new options object that includes the provided options (if any)
 * and fills in default components if required components are not defined.
 * Required can be "date", "time", or "any".
 * Defaults can be "date", "time", or "all".
 *
 * Spec: ECMAScript Internationalization API Specification, 12.1.1.
 */
function ToDateTimeOptions(options, required, defaults) {
    assert(typeof required === "string", "ToDateTimeOptions");
    assert(typeof defaults === "string", "ToDateTimeOptions");

    // Steps 1-3.
    if (options === undefined)
        options = null;
    else
        options = ToObject(options);
    options = std_Object_create(options);

    // Step 4.
    var needDefaults = true;

    // Step 5.
    if ((required === "date" || required === "any") &&
        (options.weekday !== undefined || options.year !== undefined ||
         options.month !== undefined || options.day !== undefined))
    {
        needDefaults = false;
    }

    // Step 6.
    if ((required === "time" || required === "any") &&
        (options.hour !== undefined || options.minute !== undefined ||
         options.second !== undefined))
    {
        needDefaults = false;
    }

    // Step 7.
    if (needDefaults && (defaults === "date" || defaults === "all")) {
        // The specification says to call [[DefineOwnProperty]] with false for
        // the Throw parameter, while Object.defineProperty uses true. For the
        // calls here, the difference doesn't matter because we're adding
        // properties to a new object.
        _DefineDataProperty(options, "year", "numeric");
        _DefineDataProperty(options, "month", "numeric");
        _DefineDataProperty(options, "day", "numeric");
    }

    // Step 8.
    if (needDefaults && (defaults === "time" || defaults === "all")) {
        // See comment for step 7.
        _DefineDataProperty(options, "hour", "numeric");
        _DefineDataProperty(options, "minute", "numeric");
        _DefineDataProperty(options, "second", "numeric");
    }

    // Step 9.
    return options;
}


/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.2.2.
 */
function Intl_DateTimeFormat_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    var availableLocales = callFunction(dateTimeFormatInternalProperties.availableLocales,
                                        dateTimeFormatInternalProperties);
    var requestedLocales = CanonicalizeLocaleList(locales);
    return SupportedLocales(availableLocales, requestedLocales, options);
}


/**
 * DateTimeFormat internal properties.
 *
 * Spec: ECMAScript Internationalization API Specification, 9.1 and 12.2.3.
 */
var dateTimeFormatInternalProperties = {
    localeData: dateTimeFormatLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_DateTimeFormat_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: ["ca", "nu", "hc"]
};


function dateTimeFormatLocaleData() {
    return {
        ca: intl_availableCalendars,
        nu: getNumberingSystems,
        hc: () => {
            return [null, "h11", "h12", "h23", "h24"];
        },
        default: {
            ca: intl_defaultCalendar,
            nu: intl_numberingSystem,
            hc: () => {
                return null;
            }
        }
    };
}


/**
 * Function to be bound and returned by Intl.DateTimeFormat.prototype.format.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 */
function dateTimeFormatFormatToBind(date) {
    // Step 1.
    var dtf = this;

    // Step 2.
    assert(IsObject(dtf), "dateTimeFormatFormatToBind called with non-Object");
    assert(IsDateTimeFormat(dtf), "dateTimeFormatFormatToBind called with non-DateTimeFormat");

    // Steps 3-4.
    var x = (date === undefined) ? std_Date_now() : ToNumber(date);

    // Step 5.
    return intl_FormatDateTime(dtf, x, /* formatToParts = */ false);
}

/**
 * Returns a function bound to this DateTimeFormat that returns a String value
 * representing the result of calling ToNumber(date) according to the
 * effective locale and the formatting options of this DateTimeFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 */
function Intl_DateTimeFormat_format_get() {
    // Steps 1-3.
    var dtf = UnwrapDateTimeFormat(this, "format");

    var internals = getDateTimeFormatInternals(dtf);

    // Step 4.
    if (internals.boundFormat === undefined) {
        // Step 4.a.
        var F = dateTimeFormatFormatToBind;

        // Steps 4.b-d.
        var bf = callFunction(FunctionBind, F, dtf);
        internals.boundFormat = bf;
    }

    // Step 5.
    return internals.boundFormat;
}
_SetCanonicalName(Intl_DateTimeFormat_format_get, "get format");


/**
 * Intl.DateTimeFormat.prototype.formatToParts ( date )
 */
function Intl_DateTimeFormat_formatToParts(date) {
    // Step 1.
    var dtf = this;

    // Steps 2-3.
    if (!IsObject(dtf) || !IsDateTimeFormat(dtf)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "DateTimeFormat", "formatToParts",
                       "DateTimeFormat");
    }

    // Ensure the DateTimeFormat internals are resolved.
    getDateTimeFormatInternals(dtf);

    // Steps 4-5.
    var x = (date === undefined) ? std_Date_now() : ToNumber(date);

    // Step 6.
    return intl_FormatDateTime(dtf, x, /* formatToParts = */ true);
}


/**
 * Returns the resolved options for a DateTimeFormat object.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.3 and 12.4.
 */
function Intl_DateTimeFormat_resolvedOptions() {
    // Invoke |UnwrapDateTimeFormat| per introduction of section 12.3.
    var dtf = UnwrapDateTimeFormat(this, "resolvedOptions");

    var internals = getDateTimeFormatInternals(dtf);

    var result = {
        locale: internals.locale,
        calendar: internals.calendar,
        numberingSystem: internals.numberingSystem,
        timeZone: internals.timeZone,
    };

    if (internals.mozExtensions) {
        if (internals.patternOption !== undefined) {
            result.pattern = internals.pattern;
        } else if (internals.dateStyle || internals.timeStyle) {
            result.dateStyle = internals.dateStyle;
            result.timeStyle = internals.timeStyle;
        }
    }

    resolveICUPattern(internals.pattern, result);
    return result;
}


// Table mapping ICU pattern characters back to the corresponding date-time
// components of DateTimeFormat. See
// http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
var icuPatternCharToComponent = {
    E: "weekday",
    G: "era",
    y: "year",
    M: "month",
    L: "month",
    d: "day",
    h: "hour",
    H: "hour",
    k: "hour",
    K: "hour",
    m: "minute",
    s: "second",
    z: "timeZoneName",
    v: "timeZoneName",
    V: "timeZoneName"
};


/**
 * Maps an ICU pattern string to a corresponding set of date-time components
 * and their values, and adds properties for these components to the result
 * object, which will be returned by the resolvedOptions method. For the
 * interpretation of ICU pattern characters, see
 * http://unicode.org/reports/tr35/tr35-dates.html#Date_Field_Symbol_Table
 */
function resolveICUPattern(pattern, result) {
    assert(IsObject(result), "resolveICUPattern");
    var i = 0;
    while (i < pattern.length) {
        var c = pattern[i++];
        if (c === "'") {
            while (i < pattern.length && pattern[i] !== "'")
                i++;
            i++;
        } else {
            var count = 1;
            while (i < pattern.length && pattern[i] === c) {
                i++;
                count++;
            }
            var value;
            switch (c) {
            // "text" cases
            case "G":
            case "E":
            case "z":
            case "v":
            case "V":
                if (count <= 3)
                    value = "short";
                else if (count === 4)
                    value = "long";
                else
                    value = "narrow";
                break;
            // "number" cases
            case "y":
            case "d":
            case "h":
            case "H":
            case "m":
            case "s":
            case "k":
            case "K":
                if (count === 2)
                    value = "2-digit";
                else
                    value = "numeric";
                break;
            // "text & number" cases
            case "M":
            case "L":
                if (count === 1)
                    value = "numeric";
                else if (count === 2)
                    value = "2-digit";
                else if (count === 3)
                    value = "short";
                else if (count === 4)
                    value = "long";
                else
                    value = "narrow";
                break;
            default:
                // skip other pattern characters and literal text
            }
            if (hasOwn(c, icuPatternCharToComponent))
                _DefineDataProperty(result, icuPatternCharToComponent[c], value);
            switch (c) {
            case "h":
                _DefineDataProperty(result, "hourCycle", "h12");
                _DefineDataProperty(result, "hour12", true);
                break;
            case "K":
                _DefineDataProperty(result, "hourCycle", "h11");
                _DefineDataProperty(result, "hour12", true);
                break;
            case "H":
                _DefineDataProperty(result, "hourCycle", "h23");
                _DefineDataProperty(result, "hour12", false);
                break;
            case "k":
                _DefineDataProperty(result, "hourCycle", "h24");
                _DefineDataProperty(result, "hour12", false);
                break;
            }
        }
    }
}


/********** Intl.PluralRules **********/


/**
 * PluralRules internal properties.
 *
 * Spec: ECMAScript 402 API, PluralRules, 1.3.3.
 */
var pluralRulesInternalProperties = {
    localeData: pluralRulesLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_PluralRules_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: [],
};


function pluralRulesLocaleData() {
    // PluralRules don't support any extension keys.
    return {};
}


/**
 * Compute an internal properties object from |lazyPluralRulesData|.
 */
function resolvePluralRulesInternals(lazyPluralRulesData) {
    assert(IsObject(lazyPluralRulesData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    var PluralRules = pluralRulesInternalProperties;

    // Step 13.
    const r = ResolveLocale(callFunction(PluralRules.availableLocales, PluralRules),
                          lazyPluralRulesData.requestedLocales,
                          lazyPluralRulesData.opt,
                          PluralRules.relevantExtensionKeys, PluralRules.localeData);

    // Step 14.
    internalProps.locale = r.locale;
    internalProps.type = lazyPluralRulesData.type;

    internalProps.pluralCategories = intl_GetPluralCategories(
        internalProps.locale,
        internalProps.type);

    internalProps.minimumIntegerDigits = lazyPluralRulesData.minimumIntegerDigits;
    internalProps.minimumFractionDigits = lazyPluralRulesData.minimumFractionDigits;
    internalProps.maximumFractionDigits = lazyPluralRulesData.maximumFractionDigits;

    if ("minimumSignificantDigits" in lazyPluralRulesData) {
        assert("maximumSignificantDigits" in lazyPluralRulesData, "min/max sig digits mismatch");
        internalProps.minimumSignificantDigits = lazyPluralRulesData.minimumSignificantDigits;
        internalProps.maximumSignificantDigits = lazyPluralRulesData.maximumSignificantDigits;
    }

    return internalProps;
}

/**
 * Returns an object containing the PluralRules internal properties of |obj|.
 */
function getPluralRulesInternals(obj) {
    assert(IsObject(obj), "getPluralRulesInternals called with non-object");
    assert(IsPluralRules(obj), "getPluralRulesInternals called with non-PluralRules");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "PluralRules", "bad type escaped getIntlObjectInternals");

    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    internalProps = resolvePluralRulesInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}

/**
 * Initializes an object as a PluralRules.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a PluralRules.
 * This later work occurs in |resolvePluralRulesInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript 402 API, PluralRules, 1.1.1.
 */
function InitializePluralRules(pluralRules, locales, options) {
    assert(IsObject(pluralRules), "InitializePluralRules called with non-object");
    assert(IsPluralRules(pluralRules), "InitializePluralRules called with non-PluralRules");

    // Steps 1-2 (These steps are no longer required and should be removed
    // from the spec; https://github.com/tc39/ecma402/issues/115).

    // Lazy PluralRules data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //     type: "cardinal" / "ordinal",
    //
    //     opt: // opt object computer in InitializePluralRules
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //
    //     minimumIntegerDigits: integer ∈ [1, 21],
    //     minimumFractionDigits: integer ∈ [0, 20],
    //     maximumFractionDigits: integer ∈ [0, 20],
    //
    //     // optional
    //     minimumSignificantDigits: integer ∈ [1, 21],
    //     maximumSignificantDigits: integer ∈ [1, 21],
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every PluralRules lazy data object has *all* these properties, never a
    // subset of them.
    const lazyPluralRulesData = std_Object_create(null);

    // Step 3.
    let requestedLocales = CanonicalizeLocaleList(locales);
    lazyPluralRulesData.requestedLocales = requestedLocales;

    // Steps 4-5.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Step 6.
    const type = GetOption(options, "type", "string", ["cardinal", "ordinal"], "cardinal");
    lazyPluralRulesData.type = type;

    // Step 8.
    let opt = new Record();
    lazyPluralRulesData.opt = opt;

    // Steps 9-10.
    let matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    // Steps 11-12.
    SetNumberFormatDigitOptions(lazyPluralRulesData, options, 0, 3);

    initializeIntlObject(pluralRules, "PluralRules", lazyPluralRulesData);
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript 402 API, PluralRules, 1.3.2.
 */
function Intl_PluralRules_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = callFunction(pluralRulesInternalProperties.availableLocales,
                                        pluralRulesInternalProperties);
    // Step 2.
    let requestedLocales = CanonicalizeLocaleList(locales);

    // Step 3.
    return SupportedLocales(availableLocales, requestedLocales, options);
}

/**
 * Returns a String value representing the plural category matching
 * the number passed as value according to the
 * effective locale and the formatting options of this PluralRules.
 *
 * Spec: ECMAScript 402 API, PluralRules, 1.4.3.
 */
function Intl_PluralRules_select(value) {
    // Step 1.
    let pluralRules = this;

    // Step 2.
    if (!IsObject(pluralRules) || !IsPluralRules(pluralRules))
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "PluralRules", "select", "PluralRules");

    // Ensure the PluralRules internals are resolved.
    getPluralRulesInternals(pluralRules);

    // Steps 3-4.
    let n = ToNumber(value);

    // Step 5.
    return intl_SelectPluralRule(pluralRules, n);
}

/**
 * Returns the resolved options for a PluralRules object.
 *
 * Spec: ECMAScript 402 API, PluralRules, 1.4.4.
 */
function Intl_PluralRules_resolvedOptions() {
    // Check "this PluralRules object" per introduction of section 1.4.
    if (!IsObject(this) || !IsPluralRules(this)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "PluralRules", "resolvedOptions",
                       "PluralRules");
    }

    var internals = getPluralRulesInternals(this);

    var internalsPluralCategories = internals.pluralCategories;
    var pluralCategories = [];
    for (var i = 0; i < internalsPluralCategories.length; i++)
        _DefineDataProperty(pluralCategories, i, internalsPluralCategories[i]);

    var result = {
        locale: internals.locale,
        type: internals.type,
        pluralCategories,
        minimumIntegerDigits: internals.minimumIntegerDigits,
        minimumFractionDigits: internals.minimumFractionDigits,
        maximumFractionDigits: internals.maximumFractionDigits,
    };

    var optionalProperties = [
        "minimumSignificantDigits",
        "maximumSignificantDigits"
    ];

    for (var i = 0; i < optionalProperties.length; i++) {
        var p = optionalProperties[i];
        if (hasOwn(p, internals))
            _DefineDataProperty(result, p, internals[p]);
    }
    return result;
}


/********** Intl.RelativeTimeFormat **********/

/**
 * RelativeTimeFormat internal properties.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.3.3.
 */
var relativeTimeFormatInternalProperties = {
    localeData: relativeTimeFormatLocaleData,
    _availableLocales: null,
    availableLocales: function() // eslint-disable-line object-shorthand
    {
        var locales = this._availableLocales;
        if (locales)
            return locales;

        locales = intl_RelativeTimeFormat_availableLocales();
        addSpecialMissingLanguageTags(locales);
        return (this._availableLocales = locales);
    },
    relevantExtensionKeys: [],
};

function relativeTimeFormatLocaleData() {
    // RelativeTimeFormat doesn't support any extension keys.
    return {};
}

/**
 * Compute an internal properties object from |lazyRelativeTimeFormatData|.
 */
function resolveRelativeTimeFormatInternals(lazyRelativeTimeFormatData) {
    assert(IsObject(lazyRelativeTimeFormatData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    var RelativeTimeFormat = relativeTimeFormatInternalProperties;

    // Step 16.
    const r = ResolveLocale(callFunction(RelativeTimeFormat.availableLocales, RelativeTimeFormat),
                            lazyRelativeTimeFormatData.requestedLocales,
                            lazyRelativeTimeFormatData.opt,
                            RelativeTimeFormat.relevantExtensionKeys,
                            RelativeTimeFormat.localeData);

    // Step 17.
    internalProps.locale = r.locale;
    internalProps.style = lazyRelativeTimeFormatData.style;
    internalProps.type = lazyRelativeTimeFormatData.type;

    return internalProps;
}

/**
 * Returns an object containing the RelativeTimeFormat internal properties of |obj|,
 * or throws a TypeError if |obj| isn't RelativeTimeFormat-initialized.
 */
function getRelativeTimeFormatInternals(obj, methodName) {
    assert(IsObject(obj), "getRelativeTimeFormatInternals called with non-object");
    assert(IsRelativeTimeFormat(obj), "getRelativeTimeFormatInternals called with non-RelativeTimeFormat");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "RelativeTimeFormat", "bad type escaped getIntlObjectInternals");

    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    internalProps = resolveRelativeTimeFormatInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}

/**
 * Initializes an object as a RelativeTimeFormat.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a RelativeTimeFormat.
 * This later work occurs in |resolveRelativeTimeFormatInternals|; steps not noted
 * here occur there.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.1.1.
 */
function InitializeRelativeTimeFormat(relativeTimeFormat, locales, options) {
    assert(IsObject(relativeTimeFormat),
           "InitializeRelativeimeFormat called with non-object");
    assert(IsRelativeTimeFormat(relativeTimeFormat),
           "InitializeRelativeTimeFormat called with non-RelativeTimeFormat");

    // Lazy RelativeTimeFormat data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //     style: "long" / "short" / "narrow",
    //     type: "numeric" / "text",
    //
    //     opt: // opt object computer in InitializeRelativeTimeFormat
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every RelativeTimeFormat lazy data object has *all* these properties, never a
    // subset of them.
    const lazyRelativeTimeFormatData = std_Object_create(null);

    // Step 3.
    let requestedLocales = CanonicalizeLocaleList(locales);
    lazyRelativeTimeFormatData.requestedLocales = requestedLocales;

    // Steps 4-5.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Step 6.
    let opt = new Record();

    // Steps 7-8.
    let matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    lazyRelativeTimeFormatData.opt = opt;

    // Steps 13-14.
    const style = GetOption(options, "style", "string", ["long", "short", "narrow"], "long");
    lazyRelativeTimeFormatData.style = style;

    // This option is in the process of being added to the spec.
    // See: https://github.com/tc39/proposal-intl-relative-time/issues/9
    const type = GetOption(options, "type", "string", ["numeric", "text"], "numeric");
    lazyRelativeTimeFormatData.type = type;

    initializeIntlObject(relativeTimeFormat, "RelativeTimeFormat", lazyRelativeTimeFormatData);
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.3.2.
 */
function Intl_RelativeTimeFormat_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = callFunction(relativeTimeFormatInternalProperties.availableLocales,
                                        relativeTimeFormatInternalProperties);
    // Step 2.
    let requestedLocales = CanonicalizeLocaleList(locales);

    // Step 3.
    return SupportedLocales(availableLocales, requestedLocales, options);
}

/**
 * Returns a String value representing the written form of a relative date
 * formatted according to the effective locale and the formatting options
 * of this RelativeTimeFormat object.
 *
 * Spec: ECMAScript 402 API, RelativeTImeFormat, 1.4.3.
 */
function Intl_RelativeTimeFormat_format(value, unit) {
    // Step 1.
    let relativeTimeFormat = this;

    // Step 2.
    if (!IsObject(relativeTimeFormat) || !IsRelativeTimeFormat(relativeTimeFormat))
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "RelativeTimeFormat", "format", "RelativeTimeFormat");

    // Ensure the RelativeTimeFormat internals are resolved.
    getRelativeTimeFormatInternals(relativeTimeFormat);

    // Step 3.
    let t = ToNumber(value);

    // Step 4.
    let u = ToString(unit);

    switch (u) {
      case "second":
      case "minute":
      case "hour":
      case "day":
      case "week":
      case "month":
      case "quarter":
      case "year":
        break;
      default:
        ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "unit", u);
    }

    // Step 5.
    return intl_FormatRelativeTime(relativeTimeFormat, t, u);
}

/**
 * Returns the resolved options for a PluralRules object.
 *
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.4.4.
 */
function Intl_RelativeTimeFormat_resolvedOptions() {
    // Check "this RelativeTimeFormat object" per introduction of section 1.4.
    if (!IsObject(this) || !IsRelativeTimeFormat(this)) {
        ThrowTypeError(JSMSG_INTL_OBJECT_NOT_INITED, "RelativeTimeFormat", "resolvedOptions",
                       "RelativeTimeFormat");
    }

    var internals = getRelativeTimeFormatInternals(this, "resolvedOptions");

    var result = {
        locale: internals.locale,
        style: internals.style,
        type: internals.type,
    };

    return result;
}


/********** Intl **********/


/**
 * 8.2.1 Intl.getCanonicalLocales ( locales )
 *
 * ES2017 Intl draft rev 947aa9a0c853422824a0c9510d8f09be3eb416b9
 */
function Intl_getCanonicalLocales(locales) {
    // Steps 1-2.
    return CanonicalizeLocaleList(locales);
}

/**
 * This function is a custom function in the style of the standard Intl.*
 * functions, that isn't part of any spec or proposal yet.
 *
 * Returns an object with the following properties:
 *   locale:
 *     The actual resolved locale.
 *
 *   calendar:
 *     The default calendar of the resolved locale.
 *
 *   firstDayOfWeek:
 *     The first day of the week for the resolved locale.
 *
 *   minDays:
 *     The minimum number of days in a week for the resolved locale.
 *
 *   weekendStart:
 *     The day considered the beginning of a weekend for the resolved locale.
 *
 *   weekendEnd:
 *     The day considered the end of a weekend for the resolved locale.
 *
 * Days are encoded as integers in the range 1=Sunday to 7=Saturday.
 */
function Intl_getCalendarInfo(locales) {
    // 1. Let requestLocales be ? CanonicalizeLocaleList(locales).
    const requestedLocales = CanonicalizeLocaleList(locales);

    const DateTimeFormat = dateTimeFormatInternalProperties;

    // 2. Let localeData be %DateTimeFormat%.[[localeData]].
    const localeData = DateTimeFormat.localeData;

    // 3. Let localeOpt be a new Record.
    const localeOpt = new Record();

    // 4. Set localeOpt.[[localeMatcher]] to "best fit".
    localeOpt.localeMatcher = "best fit";

    // 5. Let r be ResolveLocale(%DateTimeFormat%.[[availableLocales]],
    //    requestedLocales, localeOpt,
    //    %DateTimeFormat%.[[relevantExtensionKeys]], localeData).
    const r = ResolveLocale(callFunction(DateTimeFormat.availableLocales, DateTimeFormat),
                            requestedLocales,
                            localeOpt,
                            DateTimeFormat.relevantExtensionKeys,
                            localeData);

    // 6. Let result be GetCalendarInfo(r.[[locale]]).
    const result = intl_GetCalendarInfo(r.locale);
    _DefineDataProperty(result, "calendar", r.ca);
    _DefineDataProperty(result, "locale", r.locale);

    // 7. Return result.
    return result;
}

/**
 * This function is a custom function in the style of the standard Intl.*
 * functions, that isn't part of any spec or proposal yet.
 * We want to use it internally to retrieve translated values from CLDR in
 * order to ensure they're aligned with what Intl API returns.
 *
 * This API may one day be a foundation for an ECMA402 API spec proposal.
 *
 * The function takes two arguments - locales which is a list of locale strings
 * and options which is an object with two optional properties:
 *
 *   keys:
 *     an Array of string values that are paths to individual terms
 *
 *   style:
 *     a String with a value "long", "short" or "narrow"
 *
 * It returns an object with properties:
 *
 *   locale:
 *     a negotiated locale string
 *
 *   style:
 *     negotiated style
 *
 *   values:
 *     A key-value pair list of requested keys and corresponding
 *     translated values
 *
 */
function Intl_getDisplayNames(locales, options) {
    // 1. Let requestLocales be ? CanonicalizeLocaleList(locales).
    const requestedLocales = CanonicalizeLocaleList(locales);

    // 2. If options is undefined, then
    if (options === undefined)
        // a. Let options be ObjectCreate(null).
        options = std_Object_create(null);
    // 3. Else,
    else
        // a. Let options be ? ToObject(options).
        options = ToObject(options);

    const DateTimeFormat = dateTimeFormatInternalProperties;

    // 4. Let localeData be %DateTimeFormat%.[[localeData]].
    const localeData = DateTimeFormat.localeData;

    // 5. Let localeOpt be a new Record.
    const localeOpt = new Record();

    // 6. Set localeOpt.[[localeMatcher]] to "best fit".
    localeOpt.localeMatcher = "best fit";

    // 7. Let r be ResolveLocale(%DateTimeFormat%.[[availableLocales]], requestedLocales, localeOpt,
    //    %DateTimeFormat%.[[relevantExtensionKeys]], localeData).
    const r = ResolveLocale(callFunction(DateTimeFormat.availableLocales, DateTimeFormat),
                          requestedLocales,
                          localeOpt,
                          DateTimeFormat.relevantExtensionKeys,
                          localeData);

    // 8. Let style be ? GetOption(options, "style", "string", « "long", "short", "narrow" », "long").
    const style = GetOption(options, "style", "string", ["long", "short", "narrow"], "long");

    // 9. Let keys be ? Get(options, "keys").
    let keys = options.keys;

    // 10. If keys is undefined,
    if (keys === undefined) {
        // a. Let keys be ArrayCreate(0).
        keys = [];
    } else if (!IsObject(keys)) {
        // 11. Else,
        //   a. If Type(keys) is not Object, throw a TypeError exception.
        ThrowTypeError(JSMSG_INVALID_KEYS_TYPE);
    }

    // 12. Let processedKeys be ArrayCreate(0).
    // (This really should be a List, but we use an Array here in order that
    // |intl_ComputeDisplayNames| may infallibly access the list's length via
    // |ArrayObject::length|.)
    let processedKeys = [];

    // 13. Let len be ? ToLength(? Get(keys, "length")).
    let len = ToLength(keys.length);

    // 14. Let i be 0.
    // 15. Repeat, while i < len
    for (let i = 0; i < len; i++) {
        // a. Let processedKey be ? ToString(? Get(keys, i)).
        // b. Perform ? CreateDataPropertyOrThrow(processedKeys, i, processedKey).
        _DefineDataProperty(processedKeys, i, ToString(keys[i]));
    }

    // 16. Let names be ? ComputeDisplayNames(r.[[locale]], style, processedKeys).
    const names = intl_ComputeDisplayNames(r.locale, style, processedKeys);

    // 17. Let values be ObjectCreate(%ObjectPrototype%).
    const values = {};

    // 18. Set i to 0.
    // 19. Repeat, while i < len
    for (let i = 0; i < len; i++) {
        // a. Let key be ? Get(processedKeys, i).
        const key = processedKeys[i];
        // b. Let name be ? Get(names, i).
        const name = names[i];
        // c. Assert: Type(name) is string.
        assert(typeof name === "string", "unexpected non-string value");
        // d. Assert: the length of name is greater than zero.
        assert(name.length > 0, "empty string value");
        // e. Perform ? DefinePropertyOrThrow(values, key, name).
        _DefineDataProperty(values, key, name);
    }

    // 20. Let options be ObjectCreate(%ObjectPrototype%).
    // 21. Perform ! DefinePropertyOrThrow(result, "locale", r.[[locale]]).
    // 22. Perform ! DefinePropertyOrThrow(result, "style", style).
    // 23. Perform ! DefinePropertyOrThrow(result, "values", values).
    const result = { locale: r.locale, style, values };

    // 24. Return result.
    return result;
}

function Intl_getLocaleInfo(locales) {
  const requestedLocales = CanonicalizeLocaleList(locales);

  // In the future, we may want to expose uloc_getAvailable and use it here.
  const DateTimeFormat = dateTimeFormatInternalProperties;
  const localeData = DateTimeFormat.localeData;

  const localeOpt = new Record();
  localeOpt.localeMatcher = "best fit";

  const r = ResolveLocale(callFunction(DateTimeFormat.availableLocales, DateTimeFormat),
                          requestedLocales,
                          localeOpt,
                          DateTimeFormat.relevantExtensionKeys,
                          localeData);

  return intl_GetLocaleInfo(r.locale);
}
