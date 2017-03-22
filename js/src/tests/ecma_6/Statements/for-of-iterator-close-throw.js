function test() {
    var returnCalled = 0;
    var returnCalledExpected = 0;
    var catchEntered = 0;
    var finallyEntered = 0;
    var finallyEnteredExpected = 0;
    var iterable = {};
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            throw 42;
        }
    });

    // inner try cannot catch IteratorClose throwing
    assertThrowsValue(function() {
        for (var x of iterable) {
            try {
                return;
            } catch (e) {
                catchEntered++;
            } finally {
                finallyEntered++;
            }
        }
    }, 42);
    assertEq(returnCalled, ++returnCalledExpected);
    assertEq(catchEntered, 0);
    assertEq(finallyEntered, ++finallyEnteredExpected);
}

test();

if (typeof reportCompare === "function")
    reportCompare(0, 0);
