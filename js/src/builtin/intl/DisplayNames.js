/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Intl.DisplayNames internal properties.
 */
var displayNamesInternalProperties = {
    localeData: function() // eslint-disable-line object-shorthand
    {
        // Intl.DisplayNames doesn't support any extension keys.
        return {};
    },
    relevantExtensionKeys: []
};

var mozDisplayNamesInternalProperties = {
    localeData: function() // eslint-disable-line object-shorthand
    {
        return {
            ca: intl_availableCalendars,
            default: {
                ca: intl_defaultCalendar,
            },
        };
    },
    relevantExtensionKeys: ["ca"]
};

/**
 * Intl.DisplayNames ( [ locales [ , options ] ] )
 *
 * Compute an internal properties object from |lazyDisplayNamesData|.
 */
function resolveDisplayNamesInternals(lazyDisplayNamesData) {
    assert(IsObject(lazyDisplayNamesData), "lazy data not an object?");

    var internalProps = std_Object_create(null);

    var mozExtensions = lazyDisplayNamesData.mozExtensions;

    var DisplayNames = mozExtensions ?
                       mozDisplayNamesInternalProperties :
                       displayNamesInternalProperties;

    // Compute effective locale.

    // Step 7.
    var localeData = DisplayNames.localeData;

    // Step 10.
    var r = ResolveLocale("DisplayNames",
                          lazyDisplayNamesData.requestedLocales,
                          lazyDisplayNamesData.opt,
                          DisplayNames.relevantExtensionKeys,
                          localeData);
    // Step 12.
    internalProps.style = lazyDisplayNamesData.style;

    // Step 14.
    internalProps.type = lazyDisplayNamesData.type;

    // Step 16.
    internalProps.fallback = lazyDisplayNamesData.fallback;

    // Step 17.
    internalProps.locale = r.locale;

    if (mozExtensions) {
        internalProps.calendar = r.ca;
    }

    // The caller is responsible for associating |internalProps| with the right
    // object using |setInternalProperties|.
    return internalProps;
}

/**
 * Returns an object containing the DisplayNames internal properties of |obj|.
 */
function getDisplayNamesInternals(obj) {
    assert(IsObject(obj), "getDisplayNamesInternals called with non-object");
    assert(GuardToDisplayNames(obj) !== null, "getDisplayNamesInternals called with non-DisplayNames");

    var internals = getIntlObjectInternals(obj);
    assert(internals.type === "DisplayNames", "bad type escaped getIntlObjectInternals");

    // If internal properties have already been computed, use them.
    var internalProps = maybeInternalProperties(internals);
    if (internalProps)
        return internalProps;

    // Otherwise it's time to fully create them.
    internalProps = resolveDisplayNamesInternals(internals.lazyData);
    setInternalProperties(internals, internalProps);
    return internalProps;
}

/**
 * Intl.DisplayNames ( [ locales [ , options ] ] )
 *
 * Initializes an object as a DisplayNames.
 *
 * This method is complicated a moderate bit by its implementing initialization
 * as a *lazy* concept.  Everything that must happen now, does -- but we defer
 * all the work we can until the object is actually used as a DisplayNames.
 * This later work occurs in |resolveDisplayNamesInternals|; steps not noted
 * here occur there.
 */
function InitializeDisplayNames(displayNames, locales, options, mozExtensions) {
    assert(IsObject(displayNames), "InitializeDisplayNames called with non-object");
    assert(GuardToDisplayNames(displayNames) !== null, "InitializeDisplayNames called with non-DisplayNames");

    // Lazy DisplayNames data has the following structure:
    //
    //   {
    //     requestedLocales: List of locales,
    //
    //     opt: // opt object computed in InitializeDisplayNames
    //       {
    //         localeMatcher: "lookup" / "best fit",
    //       }
    //
    //     mozExtensions: true / false,
    //   }
    //
    // Note that lazy data is only installed as a final step of initialization,
    // so every DisplayNames lazy data object has *all* these properties, never a
    // subset of them.
    var lazyDisplayNamesData = std_Object_create(null);

    // Step 3.
    var requestedLocales = CanonicalizeLocaleList(locales);
    lazyDisplayNamesData.requestedLocales = requestedLocales;

    // Steps 4-5.
    if (options === undefined)
        options = std_Object_create(null);
    else
        options = ToObject(options);

    // Step 6.
    var opt = new Record();
    lazyDisplayNamesData.opt = opt;
    lazyDisplayNamesData.mozExtensions = mozExtensions;

    // Steps 8-9.
    var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
    opt.localeMatcher = matcher;

    if (mozExtensions) {
        var calendar = GetOption(options, "calendar", "string", undefined, undefined);

        if (calendar !== undefined) {
            calendar = intl_ValidateAndCanonicalizeUnicodeExtensionType(calendar, "calendar", "ca");
        }

        opt.ca = calendar;
    }

    // Step 11.
    var style = GetOption(options, "style", "string", ["narrow", "short", "long"], "long");

    // Step 12.
    lazyDisplayNamesData.style = style;

    // Step 13.
    var type;
    if (mozExtensions) {
        type = GetOption(options, "type", "string",
                         ["language", "region", "script", "currency", "weekday", "month",
                          "quarter", "dayPeriod", "dateTimeField"], "language");
    } else {
        type = GetOption(options, "type", "string",
                         ["language", "region", "script", "currency"], "language");
    }

    // Step 14.
    lazyDisplayNamesData.type = type;

    // Step 15.
    var fallback = GetOption(options, "fallback", "string", ["code", "none"], "code");

    // Step 16.
    lazyDisplayNamesData.fallback = fallback;

    // We've done everything that must be done now: mark the lazy data as fully
    // computed and install it.
    initializeIntlObject(displayNames, "DisplayNames", lazyDisplayNamesData);
}

/**
 * Returns the subset of the given locale list for which this locale list has a
 * matching (possibly fallback) locale. Locales appear in the same order in the
 * returned list as in the input list.
 */
function Intl_DisplayNames_supportedLocalesOf(locales /*, options*/) {
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 1.
    var availableLocales = "DisplayNames";

    // Step 2.
    var requestedLocales = CanonicalizeLocaleList(locales);

    // Step 3.
    return SupportedLocales(availableLocales, requestedLocales, options);
}

/**
 * Returns the resolved options for a DisplayNames object.
 */
function Intl_DisplayNames_of(code) {
  // Step 1.
  var displayNames = this;

  // Steps 2-3.
  if (!IsObject(displayNames) || (displayNames = GuardToDisplayNames(displayNames)) === null) {
      return callFunction(CallDisplayNamesMethodIfWrapped, this, "Intl_DisplayNames_of");
  }

  var internals = getDisplayNamesInternals(displayNames);

  // Unpack the internals object to avoid a slow runtime to selfhosted JS call
  // in |intl_ComputeDisplayName()|.
  var {locale, calendar = "", style, type} = internals;

  code = ToString(code);

  // Steps 5-7.
  var name = intl_ComputeDisplayName(displayNames, locale, calendar, style, type, code);

  // Step 8.
  // This implementation returns the empty string instead of undefined if no
  // result was found.
  if (name !== "") {
      return name;
  }

  // Step 9.
  if (internals.fallback === "code") {
      // TODO: spec should require case normalisation:
      // https://github.com/tc39/proposal-intl-displaynames/issues/72
      return code;
  }

  // Step 10.
  return undefined;
}

/**
 * Returns the resolved options for a DisplayNames object.
 */
function Intl_DisplayNames_resolvedOptions() {
    // Step 1.
    var displayNames = this;

    // Steps 2-3.
    if (!IsObject(displayNames) || (displayNames = GuardToDisplayNames(displayNames)) === null) {
        return callFunction(CallDisplayNamesMethodIfWrapped, this,
                            "Intl_DisplayNames_resolvedOptions");
    }

    var internals = getDisplayNamesInternals(displayNames);

    // Steps 4-5.
    var options = {
        locale: internals.locale,
        style: internals.style,
        type: internals.type,
        fallback: internals.fallback,
    };

    if (hasOwn("calendar", internals)) {
        options.calendar = internals.calendar;
    }

    // Step 6.
    return options;
}
