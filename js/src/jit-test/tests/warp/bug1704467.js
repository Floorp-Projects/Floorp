var always_true = true;
function f() {
    var obj = {x: 1};
    for (var i = 0; i < 100; i++) {
        var res = always_true ? obj : null;
        assertEq(res, obj);
    }
}
f();
