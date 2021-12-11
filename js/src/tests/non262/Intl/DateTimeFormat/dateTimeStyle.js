// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Date style

var dtf = new Intl.DateTimeFormat("en-US", {dateStyle: 'long'});
assertEq(dtf.resolvedOptions().dateStyle, 'long');
assertEq(dtf.resolvedOptions().hasOwnProperty('year'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('month'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('day'), false);

// Time style

var dtf = new Intl.DateTimeFormat("en-US", {timeStyle: 'long'});
assertEq(dtf.resolvedOptions().timeStyle, 'long');
assertEq(dtf.resolvedOptions().hasOwnProperty('hour'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('minute'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('second'), false);

// Date/Time style

var dtf = new Intl.DateTimeFormat("en-US", {dateStyle: 'medium', timeStyle: 'medium'});
assertEq(dtf.resolvedOptions().timeStyle, 'medium');
assertEq(dtf.resolvedOptions().dateStyle, 'medium');
assertEq(dtf.resolvedOptions().hasOwnProperty('hour'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('minute'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('second'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('year'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('month'), false);
assertEq(dtf.resolvedOptions().hasOwnProperty('day'), false);

// Error when mixing date/timeStyle with other date-time options.

assertThrowsInstanceOf(() => {
  new Intl.DateTimeFormat("en-US", {dateStyle: 'medium', year: 'numeric'});
}, TypeError);

assertThrowsInstanceOf(() => {
  new Intl.DateTimeFormat("en-US", {timeStyle: 'medium', year: 'numeric'});
}, TypeError);

// Error when using dateStyle in toLocaleTimeString.

assertThrowsInstanceOf(() => {
  new Date().toLocaleTimeString("en-US", {dateStyle: 'long'});
}, TypeError);

// Error when using dateStyle in toLocaleDateString.

assertThrowsInstanceOf(() => {
  new Date().toLocaleDateString("en-US", {timeStyle: 'long'});
}, TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');
