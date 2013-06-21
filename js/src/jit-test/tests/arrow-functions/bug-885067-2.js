(function() {
    a = (b => eval("dis(); arguments"))();
})(1, 2, 3, 4);
assertEq(a.length, 4);
