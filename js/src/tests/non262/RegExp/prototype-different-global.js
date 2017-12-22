
var otherGlobal = newGlobal();
var otherRegExp = otherGlobal.RegExp;

for (let name of ["global", "ignoreCase", "multiline", "sticky", "unicode", "source"]) {
    let getter = Object.getOwnPropertyDescriptor(RegExp.prototype, name).get;
    assertEq(typeof getter, "function");

    // Note: TypeError gets reported from wrong global!
    assertThrowsInstanceOf(() => getter.call(otherRegExp.prototype), otherGlobal.TypeError);
}

let flagsGetter = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;
assertEq(flagsGetter.call(otherRegExp.prototype), "");

assertEq(RegExp.prototype.toString.call(otherRegExp.prototype), "/(?:)/");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
