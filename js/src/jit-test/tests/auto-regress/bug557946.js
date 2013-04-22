// Binary: cache/js-dbg-64-6f1a38b94754-linux
// Flags: -j
//
/* vim: set ts=8 sts=4 et sw=4 tw=99: */

var x = 0;
var y = 0;

function h() {
    if (x == 1)
        y++;
    else
        y--;
}

function F() {
    var m = null;

    function g(i) {
        /* Force outgoing typemaps to have a string. */
        m = "badness";

        /* Loop a bit. */
        for (var i = 0; i < 10; i++) {
            h();
        }
    }

    /* Spin for a while so trees build. */
    for (var i = 0; i < 100; i++) {
        /* Capture m == TT_NULL in outgoing fi for rp[0] */
        g();

        /* Flip the switch to bail out with deep nested frames. */
        if (i > 50)
            x = 1;

        /* Set m = null on the loop tail to get better traces. */
        m = null;
    }
}

F();

