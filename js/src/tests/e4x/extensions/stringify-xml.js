// |reftest| pref(javascript.options.xml.content,true)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertEq(JSON.stringify(undefined), undefined);
assertEq(JSON.stringify(function(){}), undefined);
assertEq(JSON.stringify(<x><y></y></x>), undefined);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
