// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1093421;
var summary =
  "new Intl.NumberFormat(..., { style: 'currency', currency: '...', " +
  "currencyDisplay: 'name' or 'code' }) should have behavior other than " +
  "throwing";

print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

// Test that currencyDisplay: "code" behaves correctly and doesn't throw.

var usdCodeOptions =
  {
    style: "currency",
    currency: "USD",
    currencyDisplay: "code",
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  };
var usDollarsCode = new Intl.NumberFormat("en-US", usdCodeOptions);
assertEq(/USD/.test(usDollarsCode.format(25)), true);

// ISO 4217 currency codes are formed from an ISO 3166-1 alpha-2 country code
// followed by a third letter.  ISO 3166 guarantees that no country code
// starting with "X" will ever be assigned.  Stepping carefully around a few
// 4217-designated special "currencies", XQQ will never have a representation.
// Thus, yes: this really is specified to work, as unrecognized or unsupported
// codes pass into the string unmodified.
var xqqCodeOptions =
  {
    style: "currency",
    currency: "XQQ",
    currencyDisplay: "code",
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  };
var xqqMoneyCode = new Intl.NumberFormat("en-US", xqqCodeOptions);
assertEq(/XQQ/.test(xqqMoneyCode.format(25)), true);

// Test that currencyDisplay: "name" behaves without throwing.  (Unlike the two
// above tests, the results here aren't guaranteed as the name is
// implementation-defined.)
var usdNameOptions =
  {
    style: "currency",
    currency: "USD",
    currencyDisplay: "name",
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  };
var usDollarsName = new Intl.NumberFormat("en-US", usdNameOptions);
assertEq(usDollarsName.format(25), "25 US dollars");

// But if the implementation doesn't recognize the currency, the provided code
// is used in place of a proper name, unmolested.
var xqqNameOptions =
  {
    style: "currency",
    currency: "XQQ",
    currencyDisplay: "name",
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  };
var xqqMoneyName = new Intl.NumberFormat("en-US", xqqNameOptions);
assertEq(/XQQ/.test(xqqMoneyName.format(25)), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
