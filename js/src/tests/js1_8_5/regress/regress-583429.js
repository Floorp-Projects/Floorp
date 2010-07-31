// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Andreas Gal <gal@uci.edu>

assertEq("length" in Object.getOwnPropertyNames(Array.prototype), true);

reportCompare("ok", "ok", "bug 583429");
