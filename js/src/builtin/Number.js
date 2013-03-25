/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global intl_NumberFormat: false, */


var numberFormatCache = new Record();


/**
 * Format this Number object into a string, using the locale and formatting options
 * provided.
 *
 * Spec: ECMAScript Language Specification, 5.1 edition, 15.7.4.3.
 * Spec: ECMAScript Internationalization API Specification, 13.2.1.
 */
function Number_toLocaleString() {
    // Steps 1-2.  Note that valueOf enforces "this Number value" restrictions.
    var x = callFunction(std_Number_valueOf, this);

    // Steps 2-3.
    var locales = arguments.length > 0 ? arguments[0] : undefined;
    var options = arguments.length > 1 ? arguments[1] : undefined;

    // Step 4.
    var numberFormat;
    if (locales === undefined && options === undefined) {
        // This cache only optimizes for the old ES5 toLocaleString without
        // locales and options.
        if (numberFormatCache.numberFormat === undefined)
            numberFormatCache.numberFormat = intl_NumberFormat(locales, options);
        numberFormat = numberFormatCache.numberFormat;
    } else {
        numberFormat = intl_NumberFormat(locales, options);
    }

    // Step 5.
    return intl_FormatNumber(numberFormat, x);
}
