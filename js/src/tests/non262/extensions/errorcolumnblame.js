/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 568142;
var summary = 'error reporting blames column as well as line';

function test(f, col) {
    var caught = false;
    try {
        f();
    } catch (e) {
        caught = true;
        assertEq(e.columnNumber, col);
    }
    assertEq(caught, true);
}

/* Note single hard tab before return! */
function foo(o) {
	return o.p;
}
test(foo, 2);

//345678901234567890
test(function(f) { return f.bar; }, 20);
//       1         2
//3456789012345678901234567
test(function(f) { return f(); }, 27);
/* Cover negative colspan case using for(;;) loop with error in update part. */
test(function(){
        //         1         2         3         4
        //123456789012345678901234567890123456789012
    eval("function baz() { for (var i = 0; i < 10; i += a.b); assertEq(i !== i, true); }");
    baz();
}, 42);

//       1         2         3
//3456789012345678901234567890123456
test(function() { var tmp = null; tmp(); }, 35)
test(function() { var tmp = null;  tmp.foo; }, 36)

/* Just a generic 'throw'. */
test(function() {
//       1         2
//345678901234567890
    foo({}); throw new Error('a');
}, 20);

/* Be sure to report the right statement */
test(function() {
    function f() { return true; }
    function g() { return false; }
//       1         2
//345678901234567890123456789
    f(); g(); f(); if (f()) a += e;
}, 29);

//       1         2
//345678901234567890
test(function() { e++; }, 19);
test(function() {print += e; }, 18);
test(function(){e += 1 }, 17);
test(function() {  print[e]; }, 20);
test(function() { e[1]; }, 19);
test(function() { e(); }, 19);
test(function() { 1(); }, 19);
test(function() { Object.defineProperty() }, 19);

test(function() {
//       1         2
//34567890123456789012
    function foo() { asdf; } foo()
}, 22);

reportCompare(0, 0, "ok");

printStatus("All tests passed!");
