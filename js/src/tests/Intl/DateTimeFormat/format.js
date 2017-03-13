// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the format function with a diverse set of locales and options.
// Always use UTC to avoid dependencies on test environment.

var format;
var date = Date.UTC(2012, 11, 12, 3, 0, 0);
var longFormatOptions = {timeZone: "UTC",
                         year: "numeric", month: "long", day: "numeric",
                         hour: "numeric", minute: "numeric", second: "numeric"};

// Locale en-US; default options.
format = new Intl.DateTimeFormat("en-us", {timeZone: "UTC"});
assertEq(format.format(date), "12/12/2012");

// Locale th-TH; default options.
// Thailand uses the Buddhist calendar.
format = new Intl.DateTimeFormat("th-th", {timeZone: "UTC"});
assertEq(format.format(date), "12/12/2555");

// Locale th-TH; long format, Thai digits.
format = new Intl.DateTimeFormat("th-th-u-nu-thai", longFormatOptions);
assertEq(format.format(date), "๑๒ ธันวาคม ๒๕๕๕ ๐๓:๐๐:๐๐");

// Locale ja-JP; long format.
format = new Intl.DateTimeFormat("ja-jp", longFormatOptions);
assertEq(format.format(date), "2012年12月12日 3:00:00");

// Locale ar-MA; long format, Islamic civilian calendar.
format = new Intl.DateTimeFormat("ar-ma-u-ca-islamicc", longFormatOptions);
assertEq(format.format(date), "28 محرم، 1434 03:00:00");


// Test the .name property of the "format" getter.
var desc = Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, "format");
assertEq(desc !== undefined, true);
assertEq(typeof desc.get, "function");
assertEq(desc.get.name, "get format");


reportCompare(0, 0, 'ok');
