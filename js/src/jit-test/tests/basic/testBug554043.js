(function () {
    for (var a = 0; a < 5; a++) {
        print(-false)
        assertEq(-false, -0.0);
    }
})()
