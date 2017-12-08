var BUGNUMBER = 1168416;
var summary = "Regexp.prototype.test/exec shouldn't change lastIndex if not writable.";

print(BUGNUMBER + ": " + summary);

var regex = /0/g;
Object.freeze(regex);
var str = "abc000";

var desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assertEq(desc.writable, false);
assertEq(desc.value, 0);

assertThrowsInstanceOf(() => regex.test(str), TypeError);

desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assertEq(desc.writable, false);
assertEq(desc.value, 0);

assertThrowsInstanceOf(() => regex.exec(str), TypeError);

desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assertEq(desc.writable, false);
assertEq(desc.value, 0);

if (typeof reportCompare === "function")
    reportCompare(true, true);
