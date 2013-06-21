(function() {
    a = (b => eval("arguments"))();
})(1, 2, 3, 4);
assertEq(a.length, 0);
