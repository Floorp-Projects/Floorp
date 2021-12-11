var BUGNUMBER = 1108467;
var summary = "Implement RegExp.prototype.flags";

print(BUGNUMBER + ": " + summary);

assertEq(RegExp.prototype.flags, "");
assertEq(/foo/iymg.flags, "gimy");
assertEq(RegExp("").flags, "");
assertEq(RegExp("", "mygi").flags, "gimy");
assertEq(RegExp("", "mygui").flags, "gimuy");
assertEq(genericFlags({}), "");
assertEq(genericFlags({ignoreCase: true}), "i");
assertEq(genericFlags({sticky:1, unicode:1, global: 0}), "uy");
assertEq(genericFlags({__proto__: {multiline: true}}), "m");
assertEq(genericFlags(new Proxy({}, {get(){return true}})), "dgimsuy");

assertThrowsInstanceOf(() => genericFlags(), TypeError);
assertThrowsInstanceOf(() => genericFlags(1), TypeError);
assertThrowsInstanceOf(() => genericFlags(""), TypeError);

function genericFlags(obj) {
    return Object.getOwnPropertyDescriptor(RegExp.prototype,"flags").get.call(obj);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
