function checkPrototype(fun, proto, resolvesPrototype) {
    var desc = Object.getOwnPropertyDescriptor(fun, "prototype");
    assertEq(desc.value, proto);
    assertEq(desc.configurable, !resolvesPrototype);
    assertEq(desc.enumerable, !resolvesPrototype);
    assertEq(desc.writable, true);
}
function addPrototype(fun, proto, resolvesPrototype) {
    fun.prototype = proto;
    checkPrototype(fun, proto, resolvesPrototype);
}
function test() {
    for (var i=0; i<50; i++) {
        addPrototype(function() {}, i, true);
        addPrototype(function*() {}, i, true);
        addPrototype(function async() {}, i, true);
        // Builtins, arrow functions, bound functions don't have a default
        // prototype property.
        addPrototype(Math.abs, i, false);
        addPrototype(Array.prototype.map, i, false);
        addPrototype(() => 1, i, false);
        addPrototype((function() {}).bind(null), i, false);
    }

    // Now test this with a different IC for each function type.
    for (var i=0; i<50; i++) {
        var f = function() {};
        f.prototype = i;
        checkPrototype(f, i, true);

        f = function*() {};
        f.prototype = i;
        checkPrototype(f, i, true);

        f = function async() {};
        f.prototype = i;
        checkPrototype(f, i, true);

        Math.sin.prototype = i;
        checkPrototype(Math.sin, i, false);

        Array.prototype.filter.prototype = i;
        checkPrototype(Array.prototype.filter, i, false);

        f = () => 1;
        f.prototype = i;
        checkPrototype(f, i, false);

        f = (function() {}).bind(null);
        f.prototype = i;
        checkPrototype(f, i, false);
    }

}
test();
