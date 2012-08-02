// |reftest| pref(javascript.options.xml.content,true)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var a;
for (function::a in [1]) ;
assertEq(a, "0");

reportCompare(0, 0, 'ok');