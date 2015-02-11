if (getJitCompilerOptions()["ion.warmup.trigger"] > 50)
    setJitCompilerOption("ion.warmup.trigger", 50);

function getObjects() {
    var objs = [];

    // Own scripted getter/setter.
    objs.push({x: 0, get prop() {
        assertJitStackInvariants();
        return ++this.x;
    }, set prop(v) {
        assertJitStackInvariants();
        this.x += v;
    }});

    // Scripted getter/setter on prototype. Also verify extra formal args are
    // handled correctly.
    function getter(a, b, c) {
        assertEq(arguments.length, 0);
        assertEq(a, undefined);
        assertEq(b, undefined);
        assertEq(c, undefined);
        assertJitStackInvariants();
        bailout();
        return ++this.y;
    }
    function setter1(a, b) {
        assertEq(arguments.length, 1);
        assertEq(b, undefined);
        assertJitStackInvariants();
        this.y = a;
        bailout();
        return "unused";
    }
    var proto = {};
    Object.defineProperty(proto, "prop", {get: getter, set: setter1});
    objs.push(Object.create(proto));

    function setter2() {
        assertEq(arguments.length, 1);
        assertJitStackInvariants();
        this.y = arguments[0];
    }
    proto = {};
    Object.defineProperty(proto, "prop", {get: getter, set: setter2});
    objs.push(Object.create(proto));
    return objs;
}
function f() {
    var objs = getObjects();
    var res = 0;
    for (var i=0; i<200; i++) {
        var o = objs[i % objs.length];
        o.prop = 2;
        res += o.prop;
    }
    assertEq(res, 7233);
}
f();
