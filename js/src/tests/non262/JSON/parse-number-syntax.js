// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

testJSON('-', true);
testJSON('+', true);
testJSON('-f', true);
testJSON('+f', true);
testJSON('00', true);
testJSON('01', true);
testJSON('1.', true);
testJSON('1.0e', true);
testJSON('1.0e+', true);
testJSON('1.0e-', true);
testJSON('1.0e+z', true);
testJSON('1.0e-z', true);
testJSON('1.0ee', true);
testJSON('1.e1', true);
testJSON('1.e+1', true);
testJSON('1.e-1', true);
testJSON('.', true);
testJSON('.1', true);
testJSON('.1e', true);
testJSON('.1e1', true);
testJSON('.1e+1', true);
testJSON('.1e-1', true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
