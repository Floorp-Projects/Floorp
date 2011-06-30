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
reportCompare(true, true);
