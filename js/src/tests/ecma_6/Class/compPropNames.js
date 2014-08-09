var BUGNUMBER = 924688;
var summary = 'Computed Property Names';

print(BUGNUMBER + ": " + summary);

// Function definitions.
function syntaxError (script) {
    try {
        Function(script);
    } catch (e) {
        if (e instanceof SyntaxError) {
            return;
        }
    }
    throw new Error('Expected syntax error: ' + script);
}


// Tests begin.

assertThrowsInstanceOf(function() { var a = {[field1]: "a", [field2]: "b"}; }, ReferenceError);

assertThrowsInstanceOf(function() {
                           field1 = 1;
                           var a = {[field1]: "a", [field2]: "b"};
                       }, ReferenceError);

var f1 = 1;
var f2 = 2;
var a = {[f1]: "a", [f2]: "b"};
assertEq(a[1], "a");
assertEq(a[2], "b");

var a = {[f1]: "a", f2: "b"};
assertEq(a[1], "a");
assertEq(a.f2, "b");

var a = {["f1"]: "a", [++f2]: "b"};
assertEq(a.f1, "a");
assertEq(a[3], "b");

var a = {["f1"]: "a", f2: "b"};
assertEq(a.f1, "a");
assertEq(a.f2, "b");


var i = 0;
var a = {
    ["foo" + ++i]: i,
    ["foo" + ++i]: i,
    ["foo" + ++i]: i
};
assertEq(a.foo1, 1);
assertEq(a.foo2, 2);
assertEq(a.foo3, 3);

syntaxError("({[");
syntaxError("({[expr");
syntaxError("({[expr]");
syntaxError("({[expr]})");
syntaxError("({[expr] 0})");
syntaxError("({[expr], 0})");
syntaxError("[[expr]: 0]");
syntaxError("({[expr]: name: 0})");
syntaxError("({[1, 2]: 3})");  // because '1,2' is an Expression but not an AssignmentExpression
syntaxError("({[1;]: 1})");    // and not an ExpressionStatement
syntaxError("({[if (0) 0;]})");  // much less a Statement
syntaxError("function f() { {[x]: 1} }");  // that's not even an ObjectLiteral
syntaxError("function f() { [x]: 1 }");    // or that
syntaxError('a = {[f1@]: "a", [f2]: "b"}'); // unexpected symbol at end of AssignmentExpression
try { JSON.parse('{["a"]:4}'); } catch(e) {
    if (!(e instanceof SyntaxError)) throw new Error('Expected syntax error');
}

// Property characteristics.
a = { ["b"] : 4 };
b = Object.getOwnPropertyDescriptor(a, "b");
assertEq(b.configurable, true);
assertEq(b.enumerable, true);
assertEq(b.writable, true);
assertEq(b.value, 4);

// Setter and getter are not hit.
Object.defineProperty(Object.prototype, "x", { set: function (x) { throw "FAIL"; },
                                               get: function (x) { throw "FAIL"; } });
var a = {["x"]: 0};
assertEq(a.x, 0);

a = {["x"]: 1, ["x"]: 2};
assertEq(a.x, 2);
a = {x: 1, ["x"]: 2};
assertEq(a.x, 2);
a = {["x"]: 1, x: 2};
assertEq(a.x, 2);

// Symbols
var unique_sym = Symbol("1"), registered_sym = Symbol.for("2");
a = { [unique_sym] : 2, [registered_sym] : 3 };
assertEq(a[unique_sym], 2);
assertEq(a[registered_sym], 3);

// Same expression can be run several times to build objects with different property names.
a = [];
for (var i = 0; i < 3; i++) {
    a[i] = {["foo" + i]: i};
}
assertEq(a[0].foo0, 0);
assertEq(a[1].foo1, 1);
assertEq(a[2].foo2, 2);

// Following are stored in object's elements rather than slots.
var i = 0;
a = { [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i
}
for (var i = 1; i < 9; i++)
    assertEq(a[i], i);
syntaxError("a.1");
syntaxError("a.2");
syntaxError("a.3");
syntaxError("a.4");
syntaxError("a.5");
syntaxError("a.6");
syntaxError("a.7");
syntaxError("a.8");

// Adding a single large index.
var i = 0;
a = { [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [++i] : i,
      [3000] : 2999
}
for (var i = 1; i < 9; i++)
    assertEq(a[i], i);
assertEq(a[3000], 2999);

// Defining several properties using eval.
var code = "({";
for (i = 0; i < 1000; i++)
    code += "['foo' + " + i + "]: 'ok', "
code += "['bar']: 'ok'});";
var obj = eval(code);
for (i = 0; i < 1000; i++)
    assertEq(obj["foo" + i], "ok");
assertEq(obj["bar"], "ok");

// Can yield in a computed property name which is in a generator.
function* g() {
    var a = { [yield 1]: 2, [yield 2]: 3};
    return a;
}

var it = g();
var next = it.next();
assertEq(next.done, false);
assertEq(next.value, 1);
next = it.next("hello");
assertEq(next.done, false);
assertEq(next.value, 2);
next = it.next("world");
assertEq(next.done, true);
assertEq(next.value.hello, 2);
assertEq(next.value.world, 3);

syntaxError("a = {get [expr]() { return 3; }, set[expr](v) { return 2; }}");



reportCompare(0, 0, "ok");
