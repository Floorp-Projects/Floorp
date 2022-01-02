try {
    function testSlowArrayPopMultiFrame() {
        a = undefined;
        function parent(a, i) { i };
        function gramps(a, i) {
            return parent;
        }
        var last;
        for (var i = 0; ; gramps++) {
            last = gramps(a, i)
        }
    }(testSlowArrayPopMultiFrame(), 23);
    assertEq(0, 1);
} catch(e) {
    assertEq(e instanceof TypeError, true);
}
