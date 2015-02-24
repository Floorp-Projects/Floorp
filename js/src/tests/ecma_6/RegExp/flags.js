var BUGNUMBER = 1108467;
var summary = "Implement RegExp.prototype.flags";

print(BUGNUMBER + ": " + summary);

assertEq(RegExp.prototype.flags, "");
assertEq(/foo/iymg.flags, "gimy");
assertEq(RegExp("").flags, "");
assertEq(RegExp("", "mygi").flags, "gimy");
// TODO: Uncomment lines 12, 16, 19 and remove lines 11, 15, 18 when bug 1135377 is fixed.
assertThrowsInstanceOf(() => RegExp("", "mygui").flags, SyntaxError);
// assertEq(RegExp("", "mygui").flags, "gimuy");
assertEq(genericFlags({}), "");
assertEq(genericFlags({ignoreCase: true}), "i");
assertEq(genericFlags({sticky:1, unicode:1, global: 0}), "y");
// assertEq(genericFlags({sticky:1, unicode:1, global: 0}), "uy");
assertEq(genericFlags({__proto__: {multiline: true}}), "m");
assertEq(genericFlags(new Proxy({}, {get(){return true}})), "gimy");
// assertEq(genericFlags(new Proxy({}, {get(){return true}})), "gimuy");

assertThrowsInstanceOf(() => genericFlags(), TypeError);
assertThrowsInstanceOf(() => genericFlags(1), TypeError);
assertThrowsInstanceOf(() => genericFlags(""), TypeError);

function genericFlags(obj) {
    return Object.getOwnPropertyDescriptor(RegExp.prototype,"flags").get.call(obj);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
