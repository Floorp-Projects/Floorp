var o = {y: function() {}};
var a = [o, o, o, o, o, o, o, o, o];
Number.prototype.y = 0;
a[RECORDLOOP - 1] = 0;
try {
    for (var i = 0; i < 9; i++)
        a[i].y();
} catch (exc) {
    assertEq(exc.name, "TypeError"); // should happen when i == RECORDLOOP - 1
}
