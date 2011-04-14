// Ported from dom/src/json/test/unit/test_decode.js and
// dom/src/json/test/unit/fail13.json

testJSON('{"Numbers cannot have leading zeroes": 013}', true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
