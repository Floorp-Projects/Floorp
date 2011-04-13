// Ported from dom/src/json/test/unit/test_replacer.js

/**
 * These return* functions are used by the
 * replacer tests taken from bug 512447
 */
function returnObjectFor1(k, v)
{
  if (k == "1")
    return {};
  return v;
}
function returnArrayFor1(k, v)
{
  if (k == "1")
    return [];
  return v;
}
function returnNullFor1(k, v)
{
  if (k == "1")
    return null;
  return v;
}
function returnStringForUndefined(k, v)
{
  if (v === undefined)
    return "undefined value";
  return v;
}
var cycleObject = {}; cycleObject.cycle = cycleObject;
function returnCycleObjectFor1(k, v)
{
  if (k == "1")
    return cycleObject;
  return v;
}
var array = [0, 1, 2]; array[3] = array;
function returnCycleArrayFor1(k, v)
{
  if (k == "1")
    return array;
  return v;
}

// BEGIN TEST
var x;

x = JSON.stringify({ key: 2 },
                   function(k,v) { return k ? undefined : v; });
assertEq(x, "{}");

x = JSON.stringify(["hmm", "hmm"],
                   function(k,v) { return k !== "" ? undefined : v; });
assertEq(x, "[null,null]");

var foo = ["hmm"];
function censor(k, v)
{
  if (v !== foo)
    return "XXX";
  return v;
}
x = JSON.stringify(foo, censor);
assertEq(x, '["XXX"]');

foo = ["bar", ["baz"], "qux"];
x = JSON.stringify(foo, censor);
assertEq(x, '["XXX","XXX","XXX"]');

function censor2(k, v)
{
  if (typeof(v) == "string")
    return "XXX";
  return v;
}

foo = ["bar", ["baz"], "qux"];
x = JSON.stringify(foo, censor2);
assertEq(x, '["XXX",["XXX"],"XXX"]');

foo = { bar: 42, qux: 42, quux: 42 };
x = JSON.stringify(foo, ["bar"]);
assertEq(x, '{"bar":42}');

foo = {bar: {bar: 42, schmoo:[]}, qux: 42, quux: 42};
x = JSON.stringify(foo, ["bar", "schmoo"]);
assertEq(x, '{"bar":{"bar":42,"schmoo":[]}}');

x = JSON.stringify(foo, null, "");
assertEq(x, '{"bar":{"bar":42,"schmoo":[]},"qux":42,"quux":42}');

x = JSON.stringify(foo, null, "  ");
assertEq(x, '{\n  "bar": {\n    "bar": 42,\n    "schmoo": []\n  },\n  "qux": 42,\n  "quux": 42\n}');

foo = {bar:{bar:{}}}
x = JSON.stringify(foo, null, "  ");
assertEq(x, '{\n  "bar": {\n    "bar": {}\n  }\n}');

x = JSON.stringify({ x: 1, arr: [1] },
                   function (k,v) { return typeof v === 'number' ? 3 : v; });
assertEq(x, '{"x":3,"arr":[3]}');

foo = ['e'];
x = JSON.stringify(foo, null, '\t');
assertEq(x, '[\n\t"e"\n]');

foo = {0:0, 1:1, 2:2, 3:undefined};
x = JSON.stringify(foo, returnObjectFor1);
assertEq(x, '{"0":0,"1":{},"2":2}');

x = JSON.stringify(foo, returnArrayFor1);
assertEq(x, '{"0":0,"1":[],"2":2}');

x = JSON.stringify(foo, returnNullFor1);
assertEq(x, '{"0":0,"1":null,"2":2}');

x = JSON.stringify(foo, returnStringForUndefined);
assertEq(x, '{"0":0,"1":1,"2":2,"3":"undefined value"}');

try
{
  JSON.stringify(foo, returnCycleObjectFor1);
  throw new Error("no error thrown");
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "no TypeError thrown: " + e);
}

try
{
  JSON.stringify(foo, returnCycleArrayFor1);
  throw new Error("no error thrown");
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "no TypeError thrown: " + e);
}

foo = [0, 1, 2, undefined];
try
{
  JSON.stringify(foo, returnCycleObjectFor1);
  throw new Error("no error thrown");
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "no TypeError thrown: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
