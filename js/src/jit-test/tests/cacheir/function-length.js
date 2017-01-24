function interpreted() {
    for (var i = 0; i < 50; i++) {
        var f = function () {};
        assertEq(f.length, 0);
    }

    for (var i = 0; i < 50; i++) {
        var f = function (a, b) {};
        assertEq(f.length, 2);
    }
}

function bound() {
    for (var i = 0; i < 50; i++) {
        var f = (function () {}).bind({});
        assertEq(f.length, 0);
    }

    for (var i = 0; i < 50; i++) {
        var f = (function (a, b) {}).bind({});
        assertEq(f.length, 2);
    }
}

function native() {
    for (var i = 0; i < 50; i++) {
        // Use the interpreted function for getting the IC generated in the first place.
        var f = function (a) {};

        if (i == 15) {
            f = Math.sin;
        } else if (i == 20) {
            f = Math.cos;
        } else if (i == 25) {
            f = Math.ceil;
        } else if (i == 30) {
            f = Math.tan;
        } else if (i == 35) {
            f = Math.tanh;
        }

        assertEq(f.length, 1);
    }
}

interpreted();
bound();
native();