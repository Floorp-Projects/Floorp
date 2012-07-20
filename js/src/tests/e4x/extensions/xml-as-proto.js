// |reftest| pref(javascript.options.xml.content,true)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

try
{
  var r = Object.create(<a/>);
  throw new Error("didn't throw, got " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError for Object.create, got: " + e);
}

try
{
  var obj = {};
  obj.__proto__ = <b/>;
  throw new Error("didn't throw setting obj.__proto__");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError for obj.__proto__, got: " + e);
}

try
{
  var obj = {};
  obj.function::__proto__ = <b/>;
  throw new Error("didn't throw setting xml.function::__proto__");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError for function::__proto__, got: " + e);
}

var obj = <a/>;
var kid = obj.__proto__ = <b>c</b>;
assertEq(Object.getPrototypeOf(obj) != kid, true);
assertEq(obj.children().length(), 1);
assertEq(obj[0] == kid, true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
