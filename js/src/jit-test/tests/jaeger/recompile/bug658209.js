for (var i=0; i<20; i++) {
    (function () {
        var x;
        (function () {
            x = /abc/;
            x++;
            gc();
        })();
    })();
}
