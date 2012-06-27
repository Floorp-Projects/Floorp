// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 565604;
var summary =
  "Typed-array properties don't work when accessed from an object whose " +
  "prototype (or further-descended prototype) is a typed array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = Object.create(new Uint8Array(1));
assertEq(o.length, 1);

var o2 = Object.create(o);
assertEq(o2.length, 1);

var VARIABLE_OBJECT = {};

var props =
  [
   { property: "length", value: 1 },
   { property: "byteLength", value: 1 },
   { property: "byteOffset", value: 0 },
   { property: "buffer", value: VARIABLE_OBJECT },
  ];
for (var i = 0, sz = props.length; i < sz; i++)
{
  var p = props[i];

  var o = Object.create(new Uint8Array(1));
  var v = o[p.property];
  if (p.value !== VARIABLE_OBJECT)
    assertEq(o[p.property], p.value, "bad " + p.property + " (proto)");

  var o2 = Object.create(o);
  if (p.value !== VARIABLE_OBJECT)
    assertEq(o2[p.property], p.value, "bad " + p.property + " (grand-proto)");
  assertEq(o2[p.property], v, p.property + " mismatch");
}

reportCompare(true, true);
