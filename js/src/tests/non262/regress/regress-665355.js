var x = new ArrayBuffer(2);

var test = function(newProto) {
try {
    x.__proto__ = newProto;
    return false;
} catch(e) {
    return true;
}
}

assertEq(test(x), true);
assertEq(test({}), true);
assertEq(test(null), true);

reportCompare(true, true);
