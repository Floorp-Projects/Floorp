// |reftest| skip-if(!this.hasOwnProperty("Intl"))
// -- test that NumberFormat correctly formats 0 with various numbers of significant digits

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testData = [
    {minimumSignificantDigits: 1, maximumSignificantDigits: 1, expected: "0"},
    {minimumSignificantDigits: 1, maximumSignificantDigits: 2, expected: "0"},
    {minimumSignificantDigits: 1, maximumSignificantDigits: 3, expected: "0"},
    {minimumSignificantDigits: 1, maximumSignificantDigits: 4, expected: "0"},
    {minimumSignificantDigits: 1, maximumSignificantDigits: 5, expected: "0"},
    {minimumSignificantDigits: 2, maximumSignificantDigits: 2, expected: "0.0"},
    {minimumSignificantDigits: 2, maximumSignificantDigits: 3, expected: "0.0"},
    {minimumSignificantDigits: 2, maximumSignificantDigits: 4, expected: "0.0"},
    {minimumSignificantDigits: 2, maximumSignificantDigits: 5, expected: "0.0"},
    {minimumSignificantDigits: 3, maximumSignificantDigits: 3, expected: "0.00"},
    {minimumSignificantDigits: 3, maximumSignificantDigits: 4, expected: "0.00"},
    {minimumSignificantDigits: 3, maximumSignificantDigits: 5, expected: "0.00"},
];

for (var i = 0; i < testData.length; i++) {
    var min = testData[i].minimumSignificantDigits;
    var max = testData[i].maximumSignificantDigits;
    var options = {minimumSignificantDigits: min, maximumSignificantDigits: max};
    var format = new Intl.NumberFormat("en-US", options);
    assertEq(format.format(0), testData[i].expected,
             "Wrong formatted string for 0 with " +
             "minimumSignificantDigits " + min +
             ", maximumSignificantDigits " + max +
             ": expected \"" + expected +
             "\", actual \"" + actual + "\"");
}

reportCompare(true, true);
