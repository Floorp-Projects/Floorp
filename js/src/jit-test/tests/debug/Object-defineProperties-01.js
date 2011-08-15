// Debug.Object.prototype.defineProperties.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

var descProps = ['configurable', 'enumerable', 'writable', 'value', 'get', 'set'];
function test(objexpr, descs) {
    g.eval("obj = (" + objexpr + ");");
    var gobjw = gw.getOwnPropertyDescriptor("obj").value;
    gobjw.defineProperties(descs);

    var indirectEval = eval;
    var obj = indirectEval("(" + objexpr + ");");
    Object.defineProperties(obj, descs);

    var ids = Object.keys(descs);
    for (var i = 0; i < ids.length; i++) {
        var actual = gobjw.getOwnPropertyDescriptor(ids[i]);
        var expected = Object.getOwnPropertyDescriptor(obj, ids[i]);
        assertEq(Object.getPrototypeOf(actual), Object.prototype);
        assertEq(actual.configurable, expected.configurable);
        assertEq(actual.enumerable, expected.enumerable);
        for (var j = 0; j < descProps; j++) {
            var prop = descProps[j];
            assertEq(prop in actual, prop in expected);
            assertEq(actual[prop], expected[prop]);
        }
    }
}

test("{}", {});
test("/abc/", {});

g.eval("var aglobal = newGlobal('same-compartment');");
var aglobal = newGlobal('same-compartment');
test("aglobal", {});

var adescs = {a: {enumerable: true, writable: true, value: 0}};
test("{}", adescs);
test("{a: 1}", adescs);

var arrdescs = [{value: 'a'}, {value: 'b'}, , {value: 'd'}];
test("{}", arrdescs);
test("[]", arrdescs);
test("[0, 1, 2, 3]", arrdescs);
