var o = {y: function () {}};
var a = [o, o, o, o, o, o, o, o, o];
a[RECORDLOOP - 1] = {};
try {
    for (var i = 0; i < 9; i++)
        a[i].y();
} catch (exc) {
    assertEq(exc.name, "TypeError"); // should happen when i == RECORDLOOP - 1
}
