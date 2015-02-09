var BUGNUMBER = 1120169;
var summary = "Implement RegExp.prototype.source";

print(BUGNUMBER + ": " + summary);

assertEq(/foo/.source, "foo");
assertEq(/foo/iymg.source, "foo");
assertEq(/\//.source, "\\/");
assertEq(RegExp("/").source, "\\/");

assertThrowsInstanceOf(() => genericSource(), TypeError);
assertThrowsInstanceOf(() => genericSource(1), TypeError);
assertThrowsInstanceOf(() => genericSource(""), TypeError);
assertThrowsInstanceOf(() => genericSource({}), TypeError);
assertThrowsInstanceOf(() => genericSource(new Proxy(/foo/, {get(){ return true; }})), TypeError);

function genericSource(obj) {
    return Object.getOwnPropertyDescriptor(RegExp.prototype, "source").get.call(obj);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
