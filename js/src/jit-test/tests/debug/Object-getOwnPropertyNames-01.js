// Basic getOwnPropertyNames tests.

var g = newGlobal();
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);

function test(code) {
    code = "(" + code + ");";
    var expected = Object.getOwnPropertyNames(eval(code));
    g.eval("obj = " + code);
    var actual = gobj.getOwnPropertyDescriptor("obj").value.getOwnPropertyNames();
    assertEq(JSON.stringify(actual.sort()), JSON.stringify(expected.sort()));
}

test("{}");
test("{a: 0, b: 1}");
test("{'name with space': 0}");
test("{get x() {}, set y(v) {}}");
test("{get x() { throw 'fit'; }}");
test("Object.create({a: 1})");
test("Object.create({get a() {}, set a(v) {}})");
test("(function () { var x = {a: 0, b: 1}; delete a; return x; })()");
test("Object.create(null, {x: {value: 0}})");
test("[]");
test("[0, 1, 2]");
test("[,,,,,]");
test("/a*a/");
test("function () {}");
