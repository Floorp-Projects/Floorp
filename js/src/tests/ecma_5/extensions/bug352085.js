var a = function (t) {
    return t % 0;
}

var b = function (t) {
    return t % "";
}

var c = function (t) {
    return t % ("" + "");
}

assertEq(a.toString(), b.toString());
assertEq(a.toString(), c.toString());

if (typeof reportCompare === "function")
    reportCompare(true, true);
