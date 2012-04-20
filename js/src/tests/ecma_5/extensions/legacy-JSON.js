// |reftest| skip-if(!xulRuntime.shell) -- needs parseLegacyJSON
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

try
{
  parseLegacyJSON("[,]");
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true, "didn't get syntax error, got: " + e);
}

try
{
  parseLegacyJSON("{,}");
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true, "didn't get syntax error, got: " + e);
}

assertEq(parseLegacyJSON("[1,]").length, 1);
assertEq(parseLegacyJSON("[1, ]").length, 1);
assertEq(parseLegacyJSON("[1 , ]").length, 1);
assertEq(parseLegacyJSON("[1 ,]").length, 1);
assertEq(parseLegacyJSON("[1,2,]").length, 2);
assertEq(parseLegacyJSON("[1,2, ]").length, 2);
assertEq(parseLegacyJSON("[1,2 , ]").length, 2);
assertEq(parseLegacyJSON("[1,2 ,]").length, 2);

assertEq(parseLegacyJSON('{"a": 2,}').a, 2);
assertEq(parseLegacyJSON('{"a": 2, }').a, 2);
assertEq(parseLegacyJSON('{"a": 2 , }').a, 2);
assertEq(parseLegacyJSON('{"a": 2 ,}').a, 2);

var obj;

obj = parseLegacyJSON('{"a": 2,"b": 3,}');
assertEq(obj.a + obj.b, 5);
obj = parseLegacyJSON('{"a": 2,"b": 3, }');
assertEq(obj.a + obj.b, 5);
obj = parseLegacyJSON('{"a": 2,"b": 3 , }');
assertEq(obj.a + obj.b, 5);
obj = parseLegacyJSON('{"a": 2,"b": 3 ,}');
assertEq(obj.a + obj.b, 5);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
