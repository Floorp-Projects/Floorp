// Tests that the "return" method on iterators is called in yield*
// expressions.

function test() {
    var returnCalled = 0;
    var returnCalledExpected = 0;
    var nextCalled = 0;
    var nextCalledExpected = 0;
    var throwCalled = 0;
    var throwCalledExpected = 0;
    var iterable = {};
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            nextCalled++;
            return { done: false };
        },
        ret: function() {
            returnCalled++;
            return { done: true, value: "iter.return" };
        }
    });

    function* y() {
        yield* iterable;
    }

    // G.p.throw on an iterator without "throw" calls IteratorClose.
    var g1 = y();
    g1.next();
    assertThrowsInstanceOf(function() {
        g1.throw("foo");
    }, TypeError);
    assertEq(returnCalled, ++returnCalledExpected);
    assertEq(nextCalled, ++nextCalledExpected);
    g1.next();
    assertEq(nextCalled, nextCalledExpected);

    // G.p.return calls "return", and if the result.done is true, return the
    // result.
    var g2 = y();
    g2.next();
    var v2 = g2.return("test return");
    assertEq(v2.done, true);
    assertEq(v2.value, "iter.return");
    assertEq(returnCalled, ++returnCalledExpected);
    assertEq(nextCalled, ++nextCalledExpected);
    g2.next();
    assertEq(nextCalled, nextCalledExpected);

    // G.p.return calls "return", and if the result.done is false, continue
    // yielding.
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            nextCalled++;
            return { done: false };
        },
        ret: function() {
            returnCalled++;
            return { done: false, value: "iter.return" };
        }
    });
    var g3 = y();
    g3.next();
    var v3 = g3.return("test return");
    assertEq(v3.done, false);
    assertEq(v3.value, "iter.return");
    assertEq(returnCalled, ++returnCalledExpected);
    assertEq(nextCalled, ++nextCalledExpected);
    g3.next();
    assertEq(nextCalled, ++nextCalledExpected);

    // G.p.return throwing does not re-call iter.return.
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            throw "in iter.return";
        }
    });
    var g4 = y();
    g4.next();
    assertThrowsValue(function() {
        g4.return("in test");
    }, "in iter.return");
    assertEq(returnCalled, ++returnCalledExpected);

    // G.p.return expects iter.return to return an Object.
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            return 42;
        }
    });
    var g5 = y();
    g5.next();
    assertThrowsInstanceOf(function() {
        g5.return("foo");
    }, TypeError);
    assertEq(returnCalled, ++returnCalledExpected);

    // IteratorClose expects iter.return to return an Object.
    var g6 = y();
    g6.next();
    var exc;
    try {
        g6.throw("foo");
    } catch (e) {
        exc = e;
    } finally {
        assertEq(exc instanceof TypeError, true);
        // The message test is here because instanceof TypeError doesn't
        // distinguish the non-Object return TypeError and the
        // throw-method-is-not-defined iterator protocol error.
        assertEq(exc.toString().indexOf("non-object") > 0, true);
    }
    assertEq(returnCalled, ++returnCalledExpected);

    // G.p.return passes its argument to "return".
    iterable[Symbol.iterator] = makeIterator({
        ret: function(x) {
            assertEq(x, "in test");
            returnCalled++;
            return { done: true };
        }
    });
    var g7 = y();
    g7.next();
    g7.return("in test");
    assertEq(returnCalled, ++returnCalledExpected);

    // If a throw method is present, do not call "return".
    iterable[Symbol.iterator] = makeIterator({
        throw: function(e) {
            throwCalled++;
            throw e;
        },
        ret: function(x) {
            returnCalled++;
            return { done: true };
        }
    });
    var g8 = y();
    g8.next();
    assertThrowsValue(function() {
        g8.throw("foo");
    }, "foo");
    assertEq(throwCalled, ++throwCalledExpected);
    assertEq(returnCalled, returnCalledExpected);
}

test();

if (typeof reportCompare === "function")
    reportCompare(0, 0);
