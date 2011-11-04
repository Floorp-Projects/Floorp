// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 699705;
var summary =
  "When getting a property from a dense array that lives on the proto chain "+
  "and in particular lives on a non-native object, we should still invoke "+
  "property getter";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var protoArr = Proxy.create({
    getOwnPropertyDescriptor: function(name) {
        if (name == "foo") return { get: function() { return this[0]; } };
        return (void 0);
    },
    getPropertyDescriptor: function(name) {
        return this.getOwnPropertyDescriptor(name);
    },
}, null);
void (Array.prototype.__proto__ = protoArr);
var testArr = [ "PASS" ];
// Make sure we don't optimize the get to a .foo get, so do the dance
// with passing in a string.
function f(name) {
    return testArr[name];
}
var propValue = f("foo");
assertEq(propValue, "PASS",
	 "expected to get 'PASS' out of the getter, got: " + propValue);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
