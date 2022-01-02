function test1() {
    function getchar(s, i) {
        return s[i];
    }
    for (var i=0; i<70; i++) {
        assertEq(getchar("foo", 0), "f");
        assertEq(getchar("bar", 2), "r");
    }
    assertEq(getchar("foo", 3), undefined);
}
test1();
