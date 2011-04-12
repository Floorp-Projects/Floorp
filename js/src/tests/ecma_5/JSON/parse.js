// Ported from dom/src/json/test/unit/test_wrappers.js and
// dom/src/json/test/unit/test_decode.js

function assertIsObject(x)
{
  assertEq(typeof x, "object");
  assertEq(x instanceof Object, true);
}

function assertIsArray(x)
{
  assertIsObject(x);
  assertEq(Array.isArray(x), true);
  assertEq(Object.getPrototypeOf(x), Array.prototype);
  assertEq(x instanceof Array, true);
  assertEq(x.constructor, Array);
}

var x;
var props;

// empty object
x = JSON.parse("{}");
assertIsObject(x);
assertEq(Object.getOwnPropertyNames(x).length, 0);

// empty array
x = JSON.parse("[]");
assertIsArray(x);
assertEq(x.length, 0);

// one element array
x = JSON.parse("[[]]");
assertIsArray(x);
assertEq(x.length, 1);
assertIsArray(x[0]);
assertEq(x[0].length, 0);

// multiple arrays
x = JSON.parse("[[],[],[]]");
assertIsArray(x);
assertEq(x.length, 3);
assertIsArray(x[0]);
assertEq(x[0].length, 0);
assertIsArray(x[1]);
assertEq(x[1].length, 0);
assertIsArray(x[2]);
assertEq(x[2].length, 0);

// array key/value
x = JSON.parse('{"foo":[]}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "foo");
assertIsArray(x.foo);
assertEq(x.foo.length, 0);

x = JSON.parse('{"foo":[], "bar":[]}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x).sort();
assertEq(props.length, 2);
assertEq(props[0], "bar");
assertEq(props[1], "foo");
assertIsArray(x.foo);
assertEq(x.foo.length, 0);
assertIsArray(x.bar);
assertEq(x.bar.length, 0);

// nesting
x = JSON.parse('{"foo":[{}]}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "foo");
assertIsArray(x.foo);
assertEq(x.foo.length, 1);
assertIsObject(x.foo[0]);
assertEq(Object.getOwnPropertyNames(x.foo[0]).length, 0);

x = JSON.parse('{"foo":[{"foo":[{"foo":{}}]}]}');
assertIsObject(x.foo[0].foo[0].foo);

x = JSON.parse('{"foo":[{"foo":[{"foo":[]}]}]}');
assertIsArray(x.foo[0].foo[0].foo);

// strings
x = JSON.parse('{"foo":"bar"}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "foo");
assertEq(x.foo, "bar");

x = JSON.parse('["foo", "bar", "baz"]');
assertIsArray(x);
assertEq(x.length, 3);
assertEq(x[0], "foo");
assertEq(x[1], "bar");
assertEq(x[2], "baz");

// numbers
x = JSON.parse('{"foo":5.5, "bar":5}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x).sort();
assertEq(props.length, 2);
assertEq(props[0], "bar");
assertEq(props[1], "foo");
assertEq(x.foo, 5.5);
assertEq(x.bar, 5);

// keywords
x = JSON.parse('{"foo": true, "bar":false, "baz":null}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x).sort();
assertEq(props.length, 3);
assertEq(props[0], "bar");
assertEq(props[1], "baz");
assertEq(props[2], "foo");
assertEq(x.foo, true);
assertEq(x.bar, false);
assertEq(x.baz, null);

// short escapes
x = JSON.parse('{"foo": "\\"", "bar":"\\\\", "baz":"\\b","qux":"\\f", "quux":"\\n", "quuux":"\\r","quuuux":"\\t"}');
props = Object.getOwnPropertyNames(x).sort();
assertEq(props.length, 7);
assertEq(props[0], "bar");
assertEq(props[1], "baz");
assertEq(props[2], "foo");
assertEq(props[3], "quuuux");
assertEq(props[4], "quuux");
assertEq(props[5], "quux");
assertEq(props[6], "qux");
assertEq(x.foo, '"');
assertEq(x.bar, '\\');
assertEq(x.baz, '\b');
assertEq(x.qux, '\f');
assertEq(x.quux, "\n");
assertEq(x.quuux, "\r");
assertEq(x.quuuux, "\t");

// unicode escape
x = JSON.parse('{"foo":"hmm\\u006dmm"}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "foo");
assertEq("hmm\u006dmm", x.foo);

x = JSON.parse('{"hmm\\u006dmm":"foo"}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "hmmmmm");
assertEq(x.hmm\u006dmm, "foo");

// miscellaneous
x = JSON.parse('{"JSON Test Pattern pass3": {"The outermost value": "must be an object or array.","In this test": "It is an object." }}');
assertIsObject(x);
props = Object.getOwnPropertyNames(x);
assertEq(props.length, 1);
assertEq(props[0], "JSON Test Pattern pass3");
assertIsObject(x["JSON Test Pattern pass3"]);
props = Object.getOwnPropertyNames(x["JSON Test Pattern pass3"]).sort();
assertEq(props.length, 2);
assertEq(props[0], "In this test");
assertEq(props[1], "The outermost value");
assertEq(x["JSON Test Pattern pass3"]["The outermost value"],
         "must be an object or array.");
assertEq(x["JSON Test Pattern pass3"]["In this test"], "It is an object.");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
