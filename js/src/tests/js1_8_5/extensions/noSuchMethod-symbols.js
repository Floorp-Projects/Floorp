// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

if (typeof Symbol === "function") {
    var sym = Symbol("method");
    var hits = 0;
    var obj = {
        __noSuchMethod__: function (key, args) {
            assertEq(key, sym);
            assertEq(args.length, 2);
            assertEq(args[0], "hello");
            assertEq(args[1], "world");
            hits++;
        }
    };
    obj[sym]("hello", "world");
    assertEq(hits, 1);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
