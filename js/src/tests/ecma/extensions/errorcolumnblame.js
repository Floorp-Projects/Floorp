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
test(foo, 1);

//234567890123456789
test(function(f) { return f.bar; }, 19);
test(function(f) { return f(); }, 19);
/* Cover negative colspan case using for(;;) loop with error in update part. */
test(function(){
        //0         1         2         3         4
        //012345678901234567890123456789012345678901
    eval("function baz() { for (var i = 0; i < 10; i += a.b); assertEq(i !== i, true); }");
    baz();
}, 41);

//        1         2         3
//234567890123456789012345678901234
test(function() { var tmp = null; tmp(); }, 34)
test(function() { var tmp = null;  tmp.foo; }, 35)

/* Just a generic 'throw'. */
test(function() {
//234567890123
    foo({}); throw new Error('a');
}, 13);

/* Be sure to report the right statement */
test(function() {
    function f() { return true; }
    function g() { return false; }
//234567890123456789012345678
    f(); g(); f(); if (f()) a += e;
}, 28);

//2345678901234567890
test(function() { e++; }, 18);
test(function() {print += e; }, 17);
test(function(){e += 1 }, 16);
test(function() {  print[e]; }, 19);
test(function() { e[1]; }, 18);
test(function() { e(); }, 18);
test(function() { 1(); }, 18);
test(function() { Object.defineProperty() }, 18);

test(function() {
//23456789012345678901
    function foo() { asdf; } foo()
}, 21);

reportCompare(0, 0, "ok");

printStatus("All tests passed!");
