gczeal(0);
startgc(1, "shrinking");
for (var i=0; i<100; i++) {
    gcslice(100000);
    var s = "abcdefghi0";
    assertEq(newString(s, {maybeExternal: true}), s);
}
