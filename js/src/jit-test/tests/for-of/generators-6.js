// Named break closes the right generator-iterators.

function g() {
    for (;;)
        yield 1;
}

function isClosed(it) {
    try {
        it.next();
        return false;
    } catch (exc) {
        if (exc !== StopIteration)
            throw exc;
        return true;
    }
}

var a = g(), b = g(), c = g(), d = g(), e = g(), f = g();

for (var aa of a) {
    b_: for (var bb of b) {
        c_: for (var cc of c) {
            d_: for (var dd of d) {
                e_: for (var ee of e) {
                    for (var ff of f)
                        break c_;
                }
            }
        }
        assertEq(isClosed(a), false);
        assertEq(isClosed(b), false);
        assertEq(isClosed(c), true);
        assertEq(isClosed(d), true);
        assertEq(isClosed(e), true);
        assertEq(isClosed(f), true);
        break b_;
    }
    assertEq(isClosed(a), false);
    assertEq(isClosed(b), true);
    break;
}
assertEq(isClosed(a), true);
