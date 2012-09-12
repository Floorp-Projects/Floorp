function Person(){}
function Ninja(){}
Ninja.prototype = new Person();
function House(){}

var empty = {};
var person = new Person();
var ninja = new Ninja();
var house = new House();
var string = new String();
var bindNinja = Ninja.bind({});

var array = {};
array.__proto__ = Array.prototype;
var array2 = {};
array2.__proto__ = array.prototype;

function test(v, v2) {
  return v instanceof v2;
}
function test2(v, v2) {
  return v instanceof v2;
}
function test3(v, v2) {
  return v instanceof v2;
}
function test4(v, v2) {
  return v instanceof v2;
}

// Test if specialized for object works
for (var i=0; i!=41; i++) {
  assertEq(test(person, Person), true);
  assertEq(test(empty, Person), false);
  assertEq(test(ninja, Person), true);
  assertEq(test(house, Person), false);
  assertEq(test(string, Person), false);
  assertEq(test(new bindNinja(), Person), true);
  assertEq(test(new Ninja(), bindNinja), true);
  assertEq(test(string, String), true);
  assertEq(test(array, Array), true);
  assertEq(test(empty, Object), true);
  
  // Test if bailout works
  assertEq(test(0.1, Object), false);
  
  // Should generate TypeError
  var err = false;
  try {
    test(0.1, 5);
  } catch (e) { err = true; }
  assertEq(err, true);
  
  // Should generate TypeError
  var err = false;
  try {
    test(empty, empty);
  } catch (e) { err = true; }
  assertEq(err, true);
  
  // Should generate TypeError
  var err = false;
  try {
    test(5.0, empty);
  } catch (e) { err = true; }
  assertEq(err, true);
}

// Test if specialized for non-object lhs
for (var i=0; i!=41; i++) {
  assertEq(test2(0.1, Object), false);
}

// Check if we don't regress on https://bugzilla.mozilla.org/show_bug.cgi?id=7635
function Foo() {};
theproto = {};
Foo.prototype = theproto;

for (var i=0; i!=41; i++) {
  assertEq(test3(theproto, Foo), false);
}

