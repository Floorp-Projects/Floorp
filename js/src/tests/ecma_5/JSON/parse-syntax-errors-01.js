// Ported from dom/src/json/test/unit/test_syntax_errors.js

testJSON("{}...", true);
testJSON('{"foo": truBBBB}', true);
testJSON('{foo: truBBBB}', true);
testJSON('{"foo": undefined}', true);
testJSON('{"foo": ]', true);
testJSON('{"foo', true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
