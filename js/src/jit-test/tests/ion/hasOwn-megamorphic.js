setJitCompilerOption("offthread-compilation.enable", 0);
setJitCompilerOption("baseline.warmup.trigger", 5);
setJitCompilerOption("ion.warmup.trigger", 5);

function Base() {
}

Base.prototype.foo = false;

// XXX: tried to do this with ic.force-megamorphic, but it didn't seem to want
// to work. Maybe something is being too clever for my simple test case if I
// don't put things into a megamorphic array?
let objs = [
    {a: true},
    {b: true},
    {c: true},
    {d: true},
    {e: true},
    {f: true},
    {g: true},
    new Base(),
];

function doTest(i) {
    let o = objs[i % objs.length];
    assertEq(!o.foo, true);
    assertEq(Object.hasOwn(o, "foo"), false);
}

for (var i = 0; i < 50; i++) {
    doTest(i);
}
