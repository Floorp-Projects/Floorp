// Basic tests for obj.{seal,freeze,preventExtensions,isSealed,isFrozen,isExtensible}.

var g = newGlobal("new-compartment");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.eval("function compareObjects() {\n" +
       "    assertEq(Object.isExtensible(x), Object.isExtensible(y));\n" +
       "    var xnames = Object.getOwnPropertyNames(x).sort();\n" +
       "    var ynames = Object.getOwnPropertyNames(y).sort();\n" +
       "    assertEq(xnames.length, ynames.length);\n" +
       "    for (var i = 0; i < xnames.length; i++) {\n" +
       "        assertEq(xnames[i], ynames[i]);\n" +
       "        var name = xnames[i];\n" +
       "        var xd = Object.getOwnPropertyDescriptor(x, name);\n" +
       "        var yd = Object.getOwnPropertyDescriptor(y, name);\n" +
       "        assertEq(xd.configurable, yd.configurable, code + '.' + name + ' .configurable');\n" +
       "        assertEq(xd.enumerable, yd.enumerable, code + '.' + name + ' .enumerable');\n" +
       "        assertEq(xd.writable, yd.writable, code + '.' + name + ' .writable');\n" +
       "    }\n" +
       "}\n");

function test(code) {
    g.code = code;
    g.eval("x = (" + code + ");");
    g.eval("y = (" + code + ");");
    var xw = gw.getOwnPropertyDescriptor("x").value;

    function check() {
        // The Debugger.Object seal/freeze/preventExtensions methods
        // had the same effect as the corresponding ES5 Object methods.
        g.compareObjects();

        // The Debugger.Object introspection methods agree with the ES5 Object methods.
        assertEq(xw.isExtensible(), g.Object.isExtensible(g.x), code + ' isExtensible');
        assertEq(xw.isSealed(), g.Object.isSealed(g.x), code + ' isSealed');
        assertEq(xw.isFrozen(), g.Object.isFrozen(g.x), code + ' isFrozen');
    }

    check();

    xw.preventExtensions();
    assertEq(g.Object.isExtensible(g.x), false, code + ' preventExtensions');
    g.Object.preventExtensions(g.y);
    check();

    xw.seal();
    assertEq(g.Object.isSealed(g.x), true, code + ' seal');
    g.Object.seal(g.y);
    check();

    xw.freeze();
    assertEq(g.Object.isFrozen(g.x), true, code + ' freeze');
    g.Object.freeze(g.y);
    check();
}

test("{}");
test("{a: [1], get b() { return -1; }}");
test("Object.create(null, {x: {value: 3}, y: {get: Math.min}})");
test("[]");
test("[,,,,,]");
test("[0, 1, 2]");
test("newGlobal('same-compartment')");
