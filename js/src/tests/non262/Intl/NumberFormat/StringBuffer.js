// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The implementation of the format function uses the C++ StringBuffer class,
// which changes its storage model at 32 characters, and uses it in a way which
// also means that there's no room for null-termination at this limit.
// This test makes sure that none of this affects the output.

var format = new Intl.NumberFormat("it-IT", {minimumFractionDigits: 1});

assertEq(format.format(1123123123123123123123.1), "1.123.123.123.123.123.100.000,0");
assertEq(format.format(12123123123123123123123.1), "12.123.123.123.123.122.000.000,0");
assertEq(format.format(123123123123123123123123.1), "123.123.123.123.123.120.000.000,0");

// Ensure the ICU output matches Number.prototype.toFixed.
function formatToFixed(x) {
    var mfd = format.resolvedOptions().maximumFractionDigits;
    var s = x.toFixed(mfd);

    // To keep it simple we assume |s| is always in exponential form.
    var m = s.match(/^(\d)\.(\d+)e\+(\d+)$/);
    assertEq(m !== null, true);
    s = m[1] + m[2].padEnd(m[3], "0");

    // Group digits and append fractional part.
    m = s.match(/\d{1,3}(?=(?:\d{3})*$)/g);
    assertEq(m !== null, true);
    return m.join(".") + ",0";
}

assertEq(formatToFixed(1123123123123123123123.1), "1.123.123.123.123.123.100.000,0");
assertEq(formatToFixed(12123123123123123123123.1), "12.123.123.123.123.122.000.000,0");
assertEq(formatToFixed(123123123123123123123123.1), "123.123.123.123.123.120.000.000,0");

reportCompare(0, 0, "ok");
