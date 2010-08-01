// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Andreas Gal <gal@uci.edu>

assertEq(Object.getOwnPropertyNames(Array.prototype).indexOf("length") >= 0, true);

reportCompare("ok", "ok", "bug 583429");
