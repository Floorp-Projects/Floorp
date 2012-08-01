// |reftest| pref(javascript.options.xml.content,true) skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var v;
(function::v);

function f() { }
(function::f);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
