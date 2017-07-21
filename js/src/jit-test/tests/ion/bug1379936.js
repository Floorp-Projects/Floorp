assertEq("".replace(/x/), "");
(function () {
    for (var i = 0; i < 2000; ++i) {
        assertEq(/[^]/g.exec("abc")[0], "a");
    }
})()
