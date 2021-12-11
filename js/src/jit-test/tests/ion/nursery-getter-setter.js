var threshold = getJitCompilerOptions()["ion.warmup.trigger"] + 101;
function bar(i) {
    if (!i)
        with (this) {}; // Don't inline.
    if (i === threshold)
        minorgc();
}

function f() {
    var o2 = Object.create({get foo() { return this.x; }, set foo(x) { this.x = x + 1; }});
    for (var i=0; i<2000; i++) {
        o2.foo = i;
        assertEq(o2.foo, i + 1);
        bar(i);
    }
}
f();
