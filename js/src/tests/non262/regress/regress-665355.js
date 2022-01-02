var x = new ArrayBuffer(2);

var test = function(newProto) {
try {
    x.__proto__ = newProto;
    return false;
} catch(e) {
    return true;
}
}

// assert cycle doesn't work
assertEq(test(x), true);

// works
assertEq(test({}), false);
assertEq(test(null), false);

reportCompare(true, true);
