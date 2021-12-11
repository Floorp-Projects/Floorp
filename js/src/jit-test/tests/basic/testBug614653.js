/* Bug 614653 - This test .2 seconds with the fix, 20 minutes without. */
for (var i = 0; i < 10; ++i) {
    var arr = [];
    var s = "abcdefghijklmnop";
    for (var j = 0; j < 50000; ++j) {
        s = "<" + s + ">";
        arr.push(s);
    }
    gc();
}
