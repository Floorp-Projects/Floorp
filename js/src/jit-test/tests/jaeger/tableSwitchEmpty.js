
function f(a) {
    switch(a) {
    }
    switch(a) {
        default: return 0;
    }
    assertEq(0, 1);
}

assertEq(f(), 0);
assertEq(f(0), 0);
assertEq(f(1.1), 0);

