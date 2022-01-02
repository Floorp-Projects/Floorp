function test() {
    var count = 0;
    function f(x) {
        "use strict";
        if (x) {
            Object.seal(this);
        }
        this[0] = 1;
    }
    for (var y of [1, 0, arguments, 1]) {
        try {
            var o = new f(y);
        } catch (e) {
            count++;
        }
    }
    assertEq(count, 3);
}
test();
test();
