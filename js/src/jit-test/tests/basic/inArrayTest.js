function inArrayTest() {
    var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    for (var i = 0; i < a.length; i++) {
        if (!(i in a))
            break;
    }
    return i;
}
assertEq(inArrayTest(), 10);
