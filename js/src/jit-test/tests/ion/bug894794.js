(function() {
    for (var j = 0; j < 1; ++j) {
        var r = ((0x7fffffff - (0x80000000 | 0)) | 0) % 10000;
        assertEq(r, -1);
    }
})();

