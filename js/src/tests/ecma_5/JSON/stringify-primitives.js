// sanity
var x = JSON.stringify({});
assertEq(x, "{}");

// booleans and null
x = JSON.stringify(true);
assertEq(x, "true");

x = JSON.stringify(false);
assertEq(x, "false");

x = JSON.stringify(new Boolean(false));
assertEq(x, "false");

x = JSON.stringify(null);
assertEq(x, "null");

x = JSON.stringify(1234);
assertEq(x, "1234");

x = JSON.stringify(new Number(1234));
assertEq(x, "1234");

x = JSON.stringify("asdf");
assertEq(x, '"asdf"');

x = JSON.stringify(new String("asdf"));
assertEq(x, '"asdf"');

assertEq(JSON.stringify(undefined), undefined);
assertEq(JSON.stringify(function(){}), undefined);
assertEq(JSON.stringify(JSON.stringify), undefined);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
