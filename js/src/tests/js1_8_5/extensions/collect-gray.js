// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1337209;
var summary =
  "Test gray marking";

print(BUGNUMBER + ": " + summary);

if (typeof gczeal !== 'undefined')
    gczeal(0);

grayRoot().x = Object.create(null);
addMarkObservers([grayRoot(), grayRoot().x, this, Object.create(null)]);
gc();
let marks = getMarks();
assertEq(marks[0], 'gray', 'gray root');
assertEq(marks[1], 'gray', 'object reachable from gray root');
assertEq(marks[2], 'black', 'global');
assertEq(marks[3], 'dead', 'dead object should have been collected');

grayRoot().x = 7; // Overwrite the object
gc();
marks = getMarks();
assertEq(marks[0], 'gray', 'gray root');
assertEq(marks[1], 'dead', 'object no longer reachable from gray root');
assertEq(marks[2], 'black', 'global');
assertEq(marks[3], 'dead', 'dead object should have been collected');

var wm = new WeakMap();
var global = newGlobal();

var wrapper1 = global.eval("Object.create(null)");
wrapper1.name = "wrapper1";
var value1 = Object.create(null);
wm.set(wrapper1, value1);

var wrapper2 = global.eval("Object.create(null)");
wrapper2.name = "wrapper2";
var value2 = global.eval("Object.create(null)");
wm.set(wrapper2, value2);

grayRoot().root1 = wrapper1;
grayRoot().root2 = wrapper2;
clearMarkObservers();
addMarkObservers([wrapper1, value1, wrapper2, value2]);
wrapper1 = wrapper2 = null;
value1 = value2 = null;
gc();
marks = getMarks();
assertEq(marks[0], 'gray', 'gray key 1');
assertEq(marks[1], 'gray', 'black map, gray key => gray value');
assertEq(marks[2], 'gray', 'gray key 2');
assertEq(marks[3], 'gray', 'black map, gray key => gray value');

// Blacken one of the keys
wrapper1 = grayRoot().root1;
gc();
marks = getMarks();
assertEq(marks[0], 'black', 'black key 1');
assertEq(marks[1], 'black', 'black map, black key => black value');
assertEq(marks[2], 'gray', 'gray key 2');
assertEq(marks[3], 'gray', 'black map, gray key => gray value');

// Test edges from map&delegate => key and map&key => value.
//
// In general, when a&b => x, then if both a and b are black, then x must be
// black. If either is gray and the other is marked (gray or black), then x
// must be gray (unless otherwise reachable from black.) If neither a nor b is
// marked at all, then they will not keep x alive.

clearMarkObservers();

// Black map, gray delegate => gray key

// wm is in a variable, so is black.
wm = new WeakMap();

let key = Object.create(null);
// delegate unwraps key in the 'global' compartment
global.grayRoot().delegate = key;

// Create a value and map to it from a gray key, then make the value a gray
// root.
let value = Object.create(null);
wm.set(key, value);
grayRoot().value = value;

// We are interested in the mark bits of the map, key, and value, as well as
// the mark bits of the wrapped versions in the other compartment. Note that
// the other-compartment key is the known as the key's delegate with respect to
// the weakmap.
global.addMarkObservers([wm, key, value]);
addMarkObservers([wm, key, value]);

// Key is otherwise dead in main compartment.
key = null;
// Don't want value to be marked black.
value = null;

gc(); // Update mark bits.
let [
  other_map_mark, other_key_mark, other_value_mark,
  map_mark, key_mark, value_mark
] = getMarks();
assertEq(other_map_mark, 'dead', 'nothing points to wm in other compartment');
assertEq(other_key_mark, 'gray', 'delegate should be gray');
assertEq(other_value_mark, 'dead', 'nothing points to value wrapper in other compartment');
assertEq(map_mark, 'black', 'map in var => black');
assertEq(key_mark, 'gray', 'black map, gray delegate => gray key');
assertEq(value_mark, 'gray', 'black map, gray delegate/key => gray value');

// Black map, black delegate => black key

// Blacken the delegate by pointing to it from the other global.
global.delegate = global.grayRoot().delegate;

gc();
[
  other_map_mark, other_key_mark, other_value_mark,
  map_mark, key_mark, value_mark
] = getMarks();
assertEq(other_map_mark, 'dead', 'nothing points to wm in other compartment');
assertEq(other_key_mark, 'black', 'delegate held in global.delegate');
assertEq(other_value_mark, 'dead', 'nothing points to value wrapper in other compartment');
assertEq(map_mark, 'black', 'map in var => black');
assertEq(key_mark, 'black', 'black map, black delegate => black key');
assertEq(value_mark, 'black', 'black map, black key => black value');

// Gray map, black delegate => gray key. Unfortunately, there's no way to test
// this, because a WeakMap key's delegate is its wrapper, and there is a strong
// edge from wrappers to wrappees. The jsapi-test in testGCGrayMarking, inside
// TestWeakMaps, *does* test this case.

grayRoot().map = wm;
wm = null;

gc();
[
  other_map_mark, other_key_mark, other_value_mark,
  map_mark, key_mark, value_mark
] = getMarks();
assertEq(other_map_mark, 'dead', 'nothing points to wm in other compartment');
assertEq(other_key_mark, 'black', 'delegate held in global.delegate');
assertEq(other_value_mark, 'dead', 'nothing points to value wrapper in other compartment');
assertEq(map_mark, 'gray', 'map is a gray root');
assertEq(key_mark, 'black', 'black delegate marks its key black');
assertEq(value_mark, 'gray', 'gray map, black key => gray value');

if (typeof reportCompare === "function")
  reportCompare(true, true);
