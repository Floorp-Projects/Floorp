var BUGNUMBER = 924688;
var summary = 'Computed Property Names';

print(BUGNUMBER + ": " + summary);

var key = "z";
var { [key]: foo } = { z: "bar" };
assertEq(foo, "bar");

if (typeof reportCompare === 'function')
    reportCompare(0, 0, "ok");
