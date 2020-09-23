function interpreted() {
    for (var i = 0; i < 50; i++) {
        var f = (0, function () {});
        assertEq(f.name, "");
    }

    for (var i = 0; i < 50; i++) {
        var f = function () {};
        assertEq(f.name, "f");
    }

    for (var i = 0; i < 50; i++) {
        var f = function a () {};
        assertEq(f.name, "a");
    }
}

function bound() {
    for (var i = 0; i < 50; i++) {
        var f = (function () {}).bind({});
        assertEq(f.name, "bound ");
    }

    for (var i = 0; i < 50; i++) {
        var f = (function a () {}).bind({});
        assertEq(f.name, "bound a");
    }
}

function native() {
    for (var i = 0; i < 50; i++) {
        // Use the interpreted function for getting the IC generated in the first place.
        var f = function () {};

        var name = "f";
        if (i == 15) {
            f = Math.sin;
            name = "sin";
        } else if (i == 20) {
            f = Math.cos;
            name = "cos";
        } else if (i == 25) {
            f = Math.ceil;
            name = "ceil";
        } else if (i == 30) {
            f = Math.tan;
            name = "tan";
        } else if (i == 35) {
            f = Math.tanh;
            name = "tanh";
        }

        assertEq(f.name, name);
    }
}

interpreted();
bound();
native();
