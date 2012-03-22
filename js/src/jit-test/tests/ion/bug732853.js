/* Check that f.caller is not null in inlined frames */
function foo1() {
    return foo1.caller.p;
}

function bar1() {
    foo1(0, 1);
}

bar1();
bar1();

/* Check that we recover overflow args after a bailout
 * and that we can still iterate on the stack. */
function foo2() {
    [][0]; // cause a bailout.
    dumpStack();
}

function bar2() {
    foo2(0, 1);
}

bar2();
bar2();
