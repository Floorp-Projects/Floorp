// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
var it = (x for (x in [function(){}]));
it.next();

reportCompare("no assertion failure", "no assertion failure", "See bug 515885.");
