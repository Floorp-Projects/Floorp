// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the format function with a diverse set of locales and options.
// Always use UTC to avoid dependencies on test environment.

let mozIntl = {};
addIntlExtras(mozIntl);

// Pattern

var dtf = new Intl.DateTimeFormat("en-US", {pattern: "HH:mm MM/dd/YYYY"});
var mozDtf = new mozIntl.DateTimeFormat("en-US", {pattern: "HH:mm MM/dd/YYYY"});

assertEq(dtf.resolvedOptions().hasOwnProperty('pattern'), false);
assertEq(mozDtf.resolvedOptions().pattern, "HH:mm MM/dd/YYYY");

// Date style

var dtf = new Intl.DateTimeFormat("en-US", {dateStyle: 'long'});
assertEq(mozDtf.resolvedOptions().hasOwnProperty('dateStyle'), false);

var mozDtf = new mozIntl.DateTimeFormat("en-US", {dateStyle: 'long'});
assertEq(mozDtf.resolvedOptions().dateStyle, 'long');
assertEq(mozDtf.resolvedOptions().hasOwnProperty('year'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('month'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('day'), true);

// Time style

var dtf = new Intl.DateTimeFormat("en-US", {timeStyle: 'long'});
assertEq(dtf.resolvedOptions().hasOwnProperty('dateStyle'), false);

var mozDtf = new mozIntl.DateTimeFormat("en-US", {timeStyle: 'long'});
assertEq(mozDtf.resolvedOptions().timeStyle, 'long');
assertEq(mozDtf.resolvedOptions().hasOwnProperty('hour'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('minute'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('second'), true);

// Date/Time style

var dtf = new Intl.DateTimeFormat("en-US", {timeStyle: 'medium', dateStyle: 'medium'});
assertEq(dtf.resolvedOptions().hasOwnProperty('dateStyle'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('timeStyle'), false);

var mozDtf = new mozIntl.DateTimeFormat("en-US", {dateStyle: 'medium', timeStyle: 'medium'});
assertEq(mozDtf.resolvedOptions().timeStyle, 'medium');
assertEq(mozDtf.resolvedOptions().dateStyle, 'medium');
assertEq(mozDtf.resolvedOptions().hasOwnProperty('hour'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('minute'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('second'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('year'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('month'), true);
assertEq(mozDtf.resolvedOptions().hasOwnProperty('day'), true);

reportCompare(0, 0, 'ok');
