/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[GenerateConversionToJS]
dictionary DisplayNameOptions {
  DOMString type;
  DOMString style;
  DOMString calendar;
  sequence<DOMString> keys;
};

[GenerateInit]
dictionary DisplayNameResult {
  DOMString locale;
  DOMString type;
  DOMString style;
  DOMString calendar;
  sequence<DOMString> values;
};

[GenerateInit]
dictionary LocaleInfo {
  DOMString locale;
  DOMString direction;
};

/**
 * The IntlUtils interface provides helper functions for localization.
 */
[LegacyNoInterfaceObject,
 Exposed=Window]
interface IntlUtils {
  /**
   * Helper function to retrieve the localized values for a list of requested
   * keys.
   *
   * The function takes two arguments - locales which is a list of locale
   * strings and options which is an object with four optional properties:
   *
   *   keys:
   *     an Array of string values to localize
   *
   *   type:
   *     a String with a value "language", "region", "script", "currency",
   *     "weekday", "month", "quarter", "dayPeriod", or "dateTimeField"
   *
   *   style:
   *     a String with a value "long", "abbreviated", "short", or "narrow"
   *
   *   calendar:
   *     a String to select a specific calendar type, e.g. "gregory"
   *
   * It returns an object with properties:
   *
   *   locale:
   *     a negotiated locale string
   *
   *   type:
   *     negotiated type
   *
   *   style:
   *     negotiated style
   *
   *   calendar:
   *     negotiated calendar
   *
   *   values:
   *     a list of translated values for the requested keys
   *
   */
  [Throws]
  DisplayNameResult getDisplayNames(sequence<DOMString> locales,
                                    optional DisplayNameOptions options = {});

  /**
   * Helper function to determine if the current application locale is RTL.
   *
   * The result of this function can be overriden by this pref:
   *  - `intl.l10n.pseudo`
   */
  boolean isAppLocaleRTL();
};
