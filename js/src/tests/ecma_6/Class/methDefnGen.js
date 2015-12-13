var BUGNUMBER = 924672;
var summary = 'Method Definitions - Generators'

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

syntaxError("{*a(){}}");
syntaxError("b = {*(){}");
syntaxError("b = {*{}");
syntaxError("b = {*){}");
syntaxError("b = {*({}");
syntaxError("b = {*(){");
syntaxError("b = {*()}");
syntaxError("b = {*a(");
syntaxError("b = {*a)");
syntaxError("b = {*a(}");
syntaxError("b = {*a)}");
syntaxError("b = {*a()");
syntaxError("b = {*a()}");
syntaxError("b = {*a(){}");
syntaxError("b = {*a){}");
syntaxError("b = {*a}}");
syntaxError("b = {*a{}}");
syntaxError("b = {*a({}}");
syntaxError("b = {*a@(){}}");
syntaxError("b = {*@(){}}");
syntaxError("b = {*get a(){}}");
syntaxError("b = {get *a(){}}");
syntaxError("b = {get a*(){}}");
syntaxError("b = {*set a(c){}}");
syntaxError("b = {set *a(c){}}");
syntaxError("b = {set a*(c){}}");
syntaxError("b = {*a : 1}");
syntaxError("b = {a* : 1}");
syntaxError("b = {a :* 1}");
syntaxError("b = {a*(){}}");

// Generator methods.
b = { * g() {
    var a = { [yield 1]: 2, [yield 2]: 3};
    return a;
} }
var it = b.g();
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

// prototype property
assertEq(b.g.hasOwnProperty("prototype"), true);

// Strict mode
a = {*b(c){"use strict";yield c;}};
assertEq(a.b(1).next().value, 1);
a = {*["b"](c){"use strict";return c;}};
assertEq(a.b(1).next().value, 1);

// Generators should not have [[Construct]]
a = {*g() { yield 1; }}
assertThrowsInstanceOf(() => { new a.g }, TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0, 0, "ok");
