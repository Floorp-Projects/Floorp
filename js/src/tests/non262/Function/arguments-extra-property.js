var BUGNUMBER = 1263811;
var summary = "GetElem for modified arguments shouldn't be optimized to get original argument.";

print(BUGNUMBER + ": " + summary);

function testModifyFirst() {
    function f() {
        Object.defineProperty(arguments, 1, { value: 30 });
        assertEq(arguments[1], 30);
    }
    for (let i = 0; i < 10; i++)
        f(10, 20);
}
testModifyFirst();

function testModifyLater() {
    function f() {
        var ans = 20;
        for (let i = 0; i < 10; i++) {
            if (i == 5) {
                Object.defineProperty(arguments, 1, { value: 30 });
                ans = 30;
            }
            assertEq(arguments[1], ans);
        }
    }
    for (let i = 0; i < 10; i++)
        f(10, 20);
}
testModifyLater();

if (typeof reportCompare === "function")
    reportCompare(true, true);
