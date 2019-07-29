/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Intl.Locale internal properties.
 */
var localeInternalProperties = {
    relevantExtensionKeys: ["ca", "co", "hc", "kf", "kn", "nu"],
};

/**
 * ApplyOptionsToTag( tag, options )
 */
function ApplyOptionsToTag(tagObj, options) {
    // Steps 1-2 (Already performed in caller).

    // Step 3.
    var languageOption = GetOption(options, "language", "string", undefined, undefined);

    // Step 4.
    var language;
    if (languageOption !== undefined) {
        language = parseStandaloneLanguage(languageOption);
        if (language === null)
            ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "language", languageOption);
    }

    // Step 5.
    var scriptOption = GetOption(options, "script", "string", undefined, undefined);

    // Step 6.
    var script;
    if (scriptOption !== undefined) {
        script = parseStandaloneScript(scriptOption);
        if (script === null)
            ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "script", scriptOption);
    }

    // Step 7.
    var regionOption = GetOption(options, "region", "string", undefined, undefined);

    // Step 8.
    var region;
    if (regionOption !== undefined) {
        region = parseStandaloneRegion(regionOption);
        if (region === null)
            ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "region", regionOption);
    }

    // Step 9 (Already performed in caller).

    // Return early when no subtags were modified.
    if (language === undefined && script === undefined && region === undefined)
        return;

    // Step 10.
    if (language !== undefined)
        tagObj.language = language;

    // Step 11.
    if (script !== undefined)
        tagObj.script = script;

    // Step 12.
    if (region !== undefined)
        tagObj.region = region;

    // Replacing the "language" subtag may have turned this locale tag
    // into a grandfathered language tag. For example consider the
    // case `new Intl.Locale("en-hakka", {language: "zh"})`, where
    // "zh-hakka" is a regular grandfathered language tag.
    if (language !== undefined)
        updateGrandfatheredMappings(tagObj);

    // Step 13.
    // Optimized to only update the mappings, because all other canonicalization
    // steps already happended earlier, so there's no need to repeat them here.
    updateLocaleIdMappings(tagObj);
}

/**
 * ApplyUnicodeExtensionToTag( tag, options, relevantExtensionKeys )
 */
function ApplyUnicodeExtensionToTag(tagObj, options, relevantExtensionKeys) {
    // Steps 1-2 (Not applicable).

    // Step 3.a
    // Find the Unicode extension subtag index in |tagObj.extensions|.
    var extensions = tagObj.extensions;
    var extensionIndex = -1;
    for (var i = 0; i < extensions.length; i++) {
        if (extensions[i][0] === "u") {
            extensionIndex = i;
            break;
        }
    }

    // If Unicode extensions are neither present in |tagObj| nor in |options|,
    // we can skip everything below and directly return here.
    if (extensionIndex < 0) {
        var hasUnicodeOptions = false;
        for (var i = 0; i < relevantExtensionKeys.length; i++) {
            if (options[relevantExtensionKeys[i]] !== undefined) {
                hasUnicodeOptions = true;
                break;
            }
        }
        if (!hasUnicodeOptions)
            return;
    }

    var attributes, keywords;
    if (extensionIndex >= 0) {
        // Step 3.b.
        var components = UnicodeExtensionComponents(extensions[extensionIndex]);

        // Step 3.c.
        attributes = components.attributes;

        // Step 3.d.
        keywords = components.keywords;
    } else {
        // Step 4.a.
        attributes = [];

        // Step 4.b.
        keywords = [];
    }

    // Step 5 (Not applicable).

    // Step 6.
    for (var i = 0; i < relevantExtensionKeys.length; i++) {
        var key = relevantExtensionKeys[i];

        // Step 6.a.
        var value = undefined;

        // Steps 6.b-c.
        var entry = null;
        for (var j = 0; j < keywords.length; j++) {
            if (keywords[j].key === key) {
                entry = keywords[j];
                value = entry.value;
                break;
            }
        }

        // Step 6.d.
        assert(hasOwn(key, options), "option value for each extension key present");

        // Step 6.e.
        var optionsValue = options[key];

        // Step 6.f.
        if (optionsValue !== undefined) {
            // Step 6.f.i.
            assert(typeof optionsValue === "string", "optionsValue is a string");

            // Step 6.f.ii.
            value = optionsValue;

            // Steps 6.f.iii-iv.
            if (entry !== null)
                entry.value = value;
            else
                _DefineDataProperty(keywords, keywords.length, {key, value});
        }

        // Step 6.g (Modify |options| in place).
        options[key] = value;
    }

    // Step 7 (Not applicable).

    // Step 8.
    var newExtension = CanonicalizeUnicodeExtension(attributes, keywords, true);

    // Step 9 (Inlined call to InsertUnicodeExtension).
    // If the shortcut below step 3.a wasn't taken, the Unicode extension is
    // definitely non-empty; assert this here.
    assert(newExtension !== "", "unexpected empty Unicode extension");

    // Update or add the new Unicode extension sequence.
    if (extensionIndex >= 0) {
        extensions[extensionIndex] = newExtension;
    } else {
        _DefineDataProperty(extensions, extensions.length, newExtension);

        // Extension sequences are sorted by their singleton characters.
        if (extensions.length > 1) {
            callFunction(ArraySort, extensions);
        }
    }

    // Steps 10-11 (Not applicable).
}

/**
 * Intl.Locale( tag[, options] )
 */
function InitializeLocale(locale, tag, options) {
    // Step 7.
    if (!(typeof tag === "string" || IsObject(tag)))
        ThrowTypeError(JSMSG_INVALID_LOCALES_ELEMENT);

    // Steps 8-9.
    var unboxedLocale = callFunction(unboxLocaleOrNull, tag);
    if (unboxedLocale === null)
        tag = ToString(tag);

    // Steps 10-11.
    var hasOptions = options !== undefined;
    if (hasOptions)
        options = ToObject(options);

    // Step 12.
    var tagObj;
    if (unboxedLocale === null) {
        tagObj = parseLanguageTag(tag);
        if (tagObj === null)
            ThrowRangeError(JSMSG_INVALID_LANGUAGE_TAG, tag);

        // ApplyOptionsToTag, step 9.
        CanonicalizeLanguageTagObject(tagObj);
    } else {
        tagObj = copyLanguageTagObject(unboxedLocale.locale);

        // |tagObj| is already canonicalized for Intl.Locale objects, so we can
        // skip ApplyOptionsToTag, step 9.
    }

    // Step 13.
    var opt = new Record();

    // Skip rest of step 12 and steps 14-29 if no options were passed to the
    // Intl.Locale constructor.
    if (hasOptions) {
        // Step 12.
        ApplyOptionsToTag(tagObj, options);

        // Step 14.
        var calendar = GetOption(options, "calendar", "string", undefined, undefined);

        // Step 15.
        if (calendar !== undefined) {
            var standaloneCalendar = parseStandaloneUnicodeExtensionType(calendar);
            if (standaloneCalendar === null)
                ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "calendar", calendar);

            calendar = standaloneCalendar;
        }

        // Step 16.
        opt.ca = calendar;

        // Step 17.
        var collation = GetOption(options, "collation", "string", undefined, undefined);

        // Step 18.
        if (collation !== undefined) {
            var standaloneCollation = parseStandaloneUnicodeExtensionType(collation);
            if (standaloneCollation === null)
                ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "collation", collation);

            collation = standaloneCollation;
        }

        // Step 19.
        opt.co = collation;

        // Steps 20-21.
        opt.hc = GetOption(options, "hourCycle", "string", ["h11", "h12", "h23", "h24"],
                           undefined);

        // Steps 22-23.
        opt.kf = GetOption(options, "caseFirst", "string", ["upper", "lower", "false"],
                           undefined);

        // Step 24.
        var numeric = GetOption(options, "numeric", "boolean", undefined, undefined);

        // Step 25.
        if (numeric !== undefined)
            numeric = ToString(numeric);

        // Step 26.
        opt.kn = numeric;

        // Step 27.
        var numberingSystem = GetOption(options, "numberingSystem", "string", undefined,
                                        undefined);

        // Step 28.
        if (numberingSystem !== undefined) {
            var standaloneNumberingSystem = parseStandaloneUnicodeExtensionType(numberingSystem);
            if (standaloneNumberingSystem === null)
                ThrowRangeError(JSMSG_INVALID_OPTION_VALUE, "numberingSystem", numberingSystem);

            numberingSystem = standaloneNumberingSystem;
        }

        // Step 29.
        opt.nu = numberingSystem;
    } else {
        // Steps 14-29.
        opt.ca = undefined;
        opt.co = undefined;
        opt.hc = undefined;
        opt.kf = undefined;
        opt.kn = undefined;
        opt.nu = undefined;
    }

    // Step 2.
    var {relevantExtensionKeys} = localeInternalProperties;

    // Step 30.
    if (hasOptions || unboxedLocale === null) {
        ApplyUnicodeExtensionToTag(tagObj, opt, relevantExtensionKeys);
    } else {
        // Directly copy the Unicode extension keys from the source object if
        // no options were passed to the constructor.
        opt.ca = unboxedLocale.calendar;
        opt.co = unboxedLocale.collation;
        opt.hc = unboxedLocale.hourCycle;
        opt.kf = unboxedLocale.caseFirst;
        opt.kn = unboxedLocale.numeric;
        opt.nu = unboxedLocale.numberingSystem;
    }

    // Steps 31-37.
    var internals = new Record();
    internals.locale = tagObj;
    internals.calendar = opt.ca;
    internals.collation = opt.co;
    internals.hourCycle = opt.hc;
    internals.caseFirst = opt.kf;
    internals.numeric = opt.kn === "true" || opt.kn === "";
    internals.numberingSystem = opt.nu;

    assert(UnsafeGetReservedSlot(locale, INTL_INTERNALS_OBJECT_SLOT) === null,
           "Internal slot already initialized?");
    UnsafeSetReservedSlot(locale, INTL_INTERNALS_OBJECT_SLOT, internals);
}

/**
 * Creates a new Intl.Locale object using the language tag object |tagObj|.
 * The other internal slots are copied over from |otherLocale|.
 */
function CreateLocale(tagObj, otherLocale) {
    assert(IsObject(tagObj), "CreateLocale called with non-object");
    assert(GuardToLocale(otherLocale) !== null, "CreateLocale called with non-Locale");

#ifdef DEBUG
    var localeTag = StringFromLanguageTagObject(tagObj);
    assert(localeTag === CanonicalizeLanguageTag(localeTag),
           "CreateLocale called with non-canonical language tag");
#endif

    var locInternals = getLocaleInternals(otherLocale);

    var internals = new Record();
    internals.locale = tagObj;
    internals.calendar = locInternals.calendar;
    internals.collation = locInternals.collation;
    internals.hourCycle = locInternals.hourCycle;
    internals.caseFirst = locInternals.caseFirst;
    internals.numeric = locInternals.numeric;
    internals.numberingSystem = locInternals.numberingSystem;

    var locale = intl_CreateUninitializedLocale();
    assert(UnsafeGetReservedSlot(locale, INTL_INTERNALS_OBJECT_SLOT) === null,
                                 "Internal slot already initialized?");
    UnsafeSetReservedSlot(locale, INTL_INTERNALS_OBJECT_SLOT, internals);

    return locale;
}

/**
 * Unboxes the |this| argument if it is an Intl.Locale object, otherwise
 * returns null.
 */
function unboxLocaleOrNull() {
    if (!IsObject(this))
        return null;

    var loc = GuardToLocale(this);
    if (loc !== null)
        return getLocaleInternals(loc);
    if (IsWrappedLocale(this))
        return callFunction(CallLocaleMethodIfWrapped, this, "unboxLocaleOrNull");
    return null;
}

/**
 * Creates a copy of the given language tag object.
 */
function copyLanguageTagObject(tagObj) {
    assert(IsObject(tagObj), "copyLanguageTagObject called with non-object");

    var variants = [];
    for (var i = 0; i < tagObj.variants.length; i++)
        _DefineDataProperty(variants, i, tagObj.variants[i]);

    var extensions = [];
    for (var i = 0; i < tagObj.extensions.length; i++)
        _DefineDataProperty(extensions, i, tagObj.extensions[i]);

    return {
        language: tagObj.language,
        script: tagObj.script,
        region: tagObj.region,
        variants,
        extensions,
        privateuse: tagObj.privateuse,
    };
}

function getLocaleInternals(obj) {
    assert(IsObject(obj), "getLocaleInternals called with non-object");
    assert(GuardToLocale(obj) !== null, "getLocaleInternals called with non-Locale");

    var internals = UnsafeGetReservedSlot(obj, INTL_INTERNALS_OBJECT_SLOT);
    assert(IsObject(internals), "Internal slot not initialized?");

    return internals;
}

/**
 * Intl.Locale.prototype.maximize ()
 */
function Intl_Locale_maximize() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "Intl_Locale_maximize");

    // Step 3.
    var tagObj = copyLanguageTagObject(getLocaleInternals(loc).locale);

    var maximal = intl_AddLikelySubtags(tagObj.language, tagObj.script, tagObj.region);
    tagObj.language = maximal[0];
    tagObj.script = maximal[1];
    tagObj.region = maximal[2];

    // Update mappings in case ICU returned a non-canonicalized locale.
    updateLocaleIdMappings(tagObj);

    // Step 4.
    return CreateLocale(tagObj, loc);
}

/**
 * Intl.Locale.prototype.minimize ()
 */
function Intl_Locale_minimize() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "Intl_Locale_minimize");

    // Step 3.
    var tagObj = copyLanguageTagObject(getLocaleInternals(loc).locale);

    var minimal = intl_RemoveLikelySubtags(tagObj.language, tagObj.script, tagObj.region);
    tagObj.language = minimal[0];
    tagObj.script = minimal[1];
    tagObj.region = minimal[2];

    // Update mappings in case ICU returned a non-canonicalized locale.
    updateLocaleIdMappings(tagObj);

    // Step 4.
    return CreateLocale(tagObj, loc);
}

/**
 * Intl.Locale.prototype.toString ()
 */
function Intl_Locale_toString() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "Intl_Locale_toString");

    // Step 3.
    var tagObj = getLocaleInternals(loc).locale;
    return StringFromLanguageTagObject(tagObj);
}

/**
 * get Intl.Locale.prototype.baseName
 */
function $Intl_Locale_baseName_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_baseName_get");

    // Step 3.
    var tagObj = getLocaleInternals(loc).locale;

    // Step 4.
    // FIXME: spec bug - unicode_locale_id is always matched.

    // Step 5.
    // FIXME: spec bug - subtag production names not updated.
    var baseName = tagObj.language;

    if (tagObj.script !== undefined)
        baseName += "-" + tagObj.script;
    if (tagObj.region !== undefined)
        baseName += "-" + tagObj.region;
    if (tagObj.variants.length > 0)
        baseName += "-" + callFunction(std_Array_join, tagObj.variants, "-");

    return baseName;
}
_SetCanonicalName($Intl_Locale_baseName_get, "get baseName");

/**
 * get Intl.Locale.prototype.calendar
 */
function $Intl_Locale_calendar_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_calendar_get");

    // Step 3.
    return getLocaleInternals(loc).calendar;
}
_SetCanonicalName($Intl_Locale_calendar_get, "get calendar");

/**
 * get Intl.Locale.prototype.collation
 */
function $Intl_Locale_collation_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_collation_get");

    // Step 3.
    return getLocaleInternals(loc).collation;
}
_SetCanonicalName($Intl_Locale_collation_get, "get collation");

/**
 * get Intl.Locale.prototype.hourCycle
 */
function $Intl_Locale_hourCycle_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_hourCycle_get");

    // Step 3.
    return getLocaleInternals(loc).hourCycle;
}
_SetCanonicalName($Intl_Locale_hourCycle_get, "get hourCycle");

/**
 * get Intl.Locale.prototype.caseFirst
 */
function $Intl_Locale_caseFirst_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_caseFirst_get");

    // Step 3.
    return getLocaleInternals(loc).caseFirst;
}
_SetCanonicalName($Intl_Locale_caseFirst_get, "get caseFirst");

/**
 * get Intl.Locale.prototype.numeric
 */
function $Intl_Locale_numeric_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_numeric_get");

    // Step 3.
    return getLocaleInternals(loc).numeric;
}
_SetCanonicalName($Intl_Locale_numeric_get, "get numeric");

/**
 * get Intl.Locale.prototype.numberingSystem
 */
function $Intl_Locale_numberingSystem_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_numberingSystem_get");

    // Step 3.
    return getLocaleInternals(loc).numberingSystem;
}
_SetCanonicalName($Intl_Locale_numberingSystem_get, "get numberingSystem");

/**
 * get Intl.Locale.prototype.language
 */
function $Intl_Locale_language_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_language_get");

    // Step 3.
    var tagObj = getLocaleInternals(loc).locale;

    // Step 4 (Unnecessary assertion).

    // Step 5.
    return tagObj.language;
}
_SetCanonicalName($Intl_Locale_language_get, "get language");

/**
 * get Intl.Locale.prototype.script
 */
function $Intl_Locale_script_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_script_get");

    // Step 3.
    var tagObj = getLocaleInternals(loc).locale;

    // Step 4 (Unnecessary assertion).

    // Steps 5-6.
    // FIXME: spec bug - not all production names updated.
    return tagObj.script;
}
_SetCanonicalName($Intl_Locale_script_get, "get script");

/**
 * get Intl.Locale.prototype.region
 */
function $Intl_Locale_region_get() {
    // Step 1.
    var loc = this;

    // Step 2.
    if (!IsObject(loc) || (loc = GuardToLocale(loc)) === null)
        return callFunction(CallLocaleMethodIfWrapped, this, "$Intl_Locale_region_get");

    // Step 3.
    var tagObj = getLocaleInternals(loc).locale;

    // Step 4 (Unnecessary assertion).

    // Steps 5-6.
    // FIXME: spec bug - not all production names updated.
    return tagObj.region;
}
_SetCanonicalName($Intl_Locale_region_get, "get region");
