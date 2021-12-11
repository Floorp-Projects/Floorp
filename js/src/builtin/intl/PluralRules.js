/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PluralRules internal properties.
 *
 * Spec: ECMAScript 402 API, PluralRules, 13.3.3.
 */
var pluralRulesInternalProperties = {
    localeData: pluralRulesLocaleData,
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

    // Compute effective locale.

    // Step 10.
    var localeData = PluralRules.localeData;

    // Step 11.
    const r = ResolveLocale("PluralRules",
                            lazyPluralRulesData.requestedLocales,
                            lazyPluralRulesData.opt,
                            PluralRules.relevantExtensionKeys,
                            localeData);

    // Step 12.
    internalProps.locale = r.locale;

    // Step 8.
    internalProps.type = lazyPluralRulesData.type;

    // Step 9.
    internalProps.minimumIntegerDigits = lazyPluralRulesData.minimumIntegerDigits;

    if ("minimumFractionDigits" in lazyPluralRulesData) {
        assert("maximumFractionDigits" in lazyPluralRulesData, "min/max frac digits mismatch");
        internalProps.minimumFractionDigits = lazyPluralRulesData.minimumFractionDigits;
        internalProps.maximumFractionDigits = lazyPluralRulesData.maximumFractionDigits;
    }

    if ("minimumSignificantDigits" in lazyPluralRulesData) {
        assert("maximumSignificantDigits" in lazyPluralRulesData, "min/max sig digits mismatch");
        internalProps.minimumSignificantDigits = lazyPluralRulesData.minimumSignificantDigits;
        internalProps.maximumSignificantDigits = lazyPluralRulesData.maximumSignificantDigits;
    }

    // Intl.NumberFormat v3 Proposal
    internalProps.roundingPriority = lazyPluralRulesData.roundingPriority;

    // Step 13 (lazily computed on first access).
    internalProps.pluralCategories = null;

    return internalProps;
}

/**
 * Returns an object containing the PluralRules internal properties of |obj|.
 */
function getPluralRulesInternals(obj) {
    assert(IsObject(obj), "getPluralRulesInternals called with non-object");
    assert(intl_GuardToPluralRules(obj) !== null,
           "getPluralRulesInternals called with non-PluralRules");

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
 * Spec: ECMAScript 402 API, PluralRules, 13.1.1.
 */
function InitializePluralRules(pluralRules, locales, options) {
    assert(IsObject(pluralRules), "InitializePluralRules called with non-object");
    assert(intl_GuardToPluralRules(pluralRules) !== null,
           "InitializePluralRules called with non-PluralRules");

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
    //
    //     // optional, mutually exclusive with the significant-digits option
    //     minimumFractionDigits: integer ∈ [0, 20],
    //     maximumFractionDigits: integer ∈ [0, 20],
    //
    //     // optional, mutually exclusive with the fraction-digits option
    //     minimumSignificantDigits: integer ∈ [1, 21],
    //     maximumSignificantDigits: integer ∈ [1, 21],
    //
    //     roundingPriority: "auto" / "lessPrecision" / "morePrecision",
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every PluralRules lazy data object has *all* these properties, never a
    // subset of them.
    const lazyPluralRulesData = std_Object_create(null);

    // Step 1.
    let requestedLocales = CanonicalizeLocaleList(locales);
    lazyPluralRulesData.requestedLocales = requestedLocales;

    // Steps 2-3.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Step 4.
    let opt = new_Record();
    lazyPluralRulesData.opt = opt;

    // Steps 5-6.
    let matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    // Step 7.
    const type = GetOption(options, "type", "string", ["cardinal", "ordinal"], "cardinal");
    lazyPluralRulesData.type = type;

    // Step 9.
    SetNumberFormatDigitOptions(lazyPluralRulesData, options, 0, 3, "standard");

    // Step 15.
    //
    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(pluralRules, "PluralRules", lazyPluralRulesData);
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 *
 * Spec: ECMAScript 402 API, PluralRules, 13.3.2.
 */
function Intl_PluralRules_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = "PluralRules";

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
 * Spec: ECMAScript 402 API, PluralRules, 13.4.3.
 */
function Intl_PluralRules_select(value) {
    // Step 1.
    let pluralRules = this;

    // Steps 2-3.
    if (!IsObject(pluralRules) || (pluralRules = intl_GuardToPluralRules(pluralRules)) === null) {
        return callFunction(intl_CallPluralRulesMethodIfWrapped, this, value,
                            "Intl_PluralRules_select");
    }

    // Step 4.
    let n = ToNumber(value);

    // Ensure the PluralRules internals are resolved.
    getPluralRulesInternals(pluralRules);

    // Step 5.
    return intl_SelectPluralRule(pluralRules, n);
}

/**
 * Returns a String value representing the plural category matching the input
 * number range according to the effective locale and the formatting options
 * of this PluralRules.
 */
function Intl_PluralRules_selectRange(start, end) {
    // Step 1.
    var pluralRules = this;

    // Step 2.
    if (!IsObject(pluralRules) || (pluralRules = intl_GuardToPluralRules(pluralRules)) === null) {
        return callFunction(intl_CallPluralRulesMethodIfWrapped, this, start, end,
                            "Intl_PluralRules_selectRange");
    }

    // Step 3.
    var x = ToNumber(start);

    // Step 4.
    var y = ToNumber(end);

    // Step 5.
    return intl_SelectPluralRuleRange(pluralRules, x, y);
}

/**
 * Returns the resolved options for a PluralRules object.
 *
 * Spec: ECMAScript 402 API, PluralRules, 13.4.4.
 */
function Intl_PluralRules_resolvedOptions() {
    // Step 1.
    var pluralRules = this;

    // Steps 2-3.
    if (!IsObject(pluralRules) || (pluralRules = intl_GuardToPluralRules(pluralRules)) === null) {
        return callFunction(intl_CallPluralRulesMethodIfWrapped, this,
                            "Intl_PluralRules_resolvedOptions");
    }

    var internals = getPluralRulesInternals(pluralRules);

    // Steps 4-5.
    var result = {
        locale: internals.locale,
        type: internals.type,
        minimumIntegerDigits: internals.minimumIntegerDigits,
    };

    // Min/Max fraction digits are either both present or not present at all.
    assert(hasOwn("minimumFractionDigits", internals) ===
           hasOwn("maximumFractionDigits", internals),
           "minimumFractionDigits is present iff maximumFractionDigits is present");

    if (hasOwn("minimumFractionDigits", internals)) {
        DefineDataProperty(result, "minimumFractionDigits", internals.minimumFractionDigits);
        DefineDataProperty(result, "maximumFractionDigits", internals.maximumFractionDigits);
    }

    // Min/Max significant digits are either both present or not present at all.
    assert(hasOwn("minimumSignificantDigits", internals) ===
           hasOwn("maximumSignificantDigits", internals),
           "minimumSignificantDigits is present iff maximumSignificantDigits is present");

    if (hasOwn("minimumSignificantDigits", internals)) {
        DefineDataProperty(result, "minimumSignificantDigits",
                           internals.minimumSignificantDigits);
        DefineDataProperty(result, "maximumSignificantDigits",
                           internals.maximumSignificantDigits);
    }

    // Step 6.
    var internalsPluralCategories = internals.pluralCategories;
    if (internalsPluralCategories === null) {
        internalsPluralCategories = intl_GetPluralCategories(pluralRules);
        internals.pluralCategories = internalsPluralCategories;
    }

    var pluralCategories = [];
    for (var i = 0; i < internalsPluralCategories.length; i++)
        DefineDataProperty(pluralCategories, i, internalsPluralCategories[i]);

    // Step 7.
    DefineDataProperty(result, "pluralCategories", pluralCategories);

#ifdef NIGHTLY_BUILD
    DefineDataProperty(result, "roundingPriority", internals.roundingPriority);
#endif

    // Step 8.
    return result;
}
