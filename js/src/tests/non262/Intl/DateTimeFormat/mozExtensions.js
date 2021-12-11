// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let mozIntl = {};
addIntlExtras(mozIntl);

// Pattern

var dtf = new Intl.DateTimeFormat("en-US", {pattern: "HH:mm MM/dd/YYYY"});
var mozDtf = new mozIntl.DateTimeFormat("en-US", {pattern: "HH:mm MM/dd/YYYY"});

assertEq(dtf.resolvedOptions().hasOwnProperty('pattern'), false);
assertEq(mozDtf.resolvedOptions().pattern, "HH:mm MM/dd/YYYY");

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');
