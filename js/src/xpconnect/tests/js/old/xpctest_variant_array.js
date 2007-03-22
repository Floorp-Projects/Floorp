
const TestVariant = Components.Constructor("@mozilla.org/js/xpc/test/TestVariant;1", 
                                           "nsITestVariant");

var tv = new TestVariant;


print("\npassThruVariant tests...\n");

print(tv.passThruVariant(["a","b","c"]));
print(tv.passThruVariant([1,2,3]));
print(tv.passThruVariant([1,2.1,3]));
print(tv.passThruVariant([{},{},{}]));
print(tv.passThruVariant([{},{},null]));
print(tv.passThruVariant([{},{},"foo"]));
print(tv.passThruVariant([[1,2,3],{},"foo"]));
print(tv.passThruVariant([[1,2,3],[4,5,6],["a","b","c"]]));

print("\ncopyVariant tests...\n");

print(tv.copyVariant(["a","b","c"]));
print(tv.copyVariant([1,2,3]));
print(tv.copyVariant([1,2.1,3]));
print(tv.copyVariant([{},{},{}]));
print(tv.copyVariant([{},{},null]));
print(tv.copyVariant([{},{},"foo"]));
print(tv.copyVariant([[1,2,3],{},"foo"]));
print(tv.copyVariant([[1,2,3],[4,5,6],["a","b","c"]]));

print("\ndone\n");
