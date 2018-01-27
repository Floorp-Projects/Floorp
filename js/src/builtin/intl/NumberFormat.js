/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Portions Copyright Norbert Lindenberg 2011-2012. */

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
