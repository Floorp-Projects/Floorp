var x = new ArrayBuffer(2);

var test = function() {
try {
    x.__proto__ = x;
    return false;
} catch(e) {
    return true;
}
}

assertEq(test(), true);

// ArrayBuffer's __proto__ behaviour verification.
var y = new ArrayBuffer();
y.__proto__ = null;
assertEq(y.__proto__, undefined);
reportCompare(true, true);
