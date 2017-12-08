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
