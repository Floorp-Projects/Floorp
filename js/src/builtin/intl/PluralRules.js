/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
