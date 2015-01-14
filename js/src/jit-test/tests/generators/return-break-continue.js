load(libdir + "iteration.js");

// break in finally.
function *f1() {
    L: try {
        yield 1;
    } finally {
        break L;
    }
    return 2;
}
it = f1();
assertIteratorNext(it, 1);
assertIteratorResult(it.return(4), 2, true);
assertIteratorDone(it);

// continue in finally, followed by return.
function *f2() {
    do try {
        yield 1;
    } catch (e) {
        assertEq(0, 1);
    } finally {
        continue;
    } while (0);
    return 2;
}
it = f2();
assertIteratorNext(it, 1);
assertIteratorResult(it.return(4), 2, true);
assertIteratorDone(it);

// continue in finally, followed by yield.
function *f3() {
    do try {
        yield 1;
    } catch (e) {
        assertEq(0, 1);
    } finally {
        continue;
    } while (0);
    yield 2;
}
it = f3();
assertIteratorNext(it, 1);
assertIteratorResult(it.return(4), 2, false);
assertIteratorDone(it);

// continue in finally.
function *f4() {
    var i = 0;
    while (true) {
        try {
            yield i++;
        } finally {
            if (i < 3)
                continue;
        }
    }
}
it = f4();
assertIteratorNext(it, 0);
assertIteratorResult(it.return(-1), 1, false);
assertIteratorResult(it.return(-2), 2, false);
assertIteratorResult(it.return(-3), -3, true);
assertIteratorDone(it);
