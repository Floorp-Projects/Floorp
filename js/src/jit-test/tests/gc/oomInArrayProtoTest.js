// |jit-test| skip-if: !('oomTest' in this)

function arrayProtoOutOfRange() {
    function f(obj) {
        return typeof obj[15];
    }

    function test() {
        var a = [1, 2];
        a.__proto__ = {15: 1337};
        var b = [1, 2, 3, 4];

        for (var i = 0; i < 200; i++) {
            var r = f(i % 2 ? a : b);
            assertEq(r, i % 2 ? "number" : "undefined");
        }
    }

    test();
}

oomTest(arrayProtoOutOfRange);
