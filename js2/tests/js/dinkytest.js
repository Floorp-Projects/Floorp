// Simple class, initialize instance variable
// outside, then return value from instance method
class C {
    var a; 
    function f() { return a; } 
}

var c:C = new C
c.a = 12
if (c.f() != 12) print("failed #1");

// make a local variable do the same
// and check the local & global aren't interacting
function t() 
{
    var c:C = new C;
    c.a = 14;
    return c.f();
}

if (t() != 14) print("failed #2");
if (c.f() != 12) print("failed #3");

// extend the class, overriding the method
dynamic class D extends C {
    var b;
    override function f() { return b + a; }
    function g() { return c; }
}

var d:C = new D;
d.a = 2;
d.b = 1;
// call the overridden method
if (d.f() != 3) print("failed #4");
d.c = 4;
// check that dynamic properties work
if (d.c != 4) print("failed #5");


// Make sure namespace declared variables aren't
// in public view
namespace NS1;
namespace NS2;

NS1 var x = 12;
NS2 var x = 16;

var test:Boolean = false;
try {
    x != 12;
}
catch (ex) {
    test = (ex.name == "ReferenceError");
}
if (!test) print("failed #6");
// and that there's no collision between them all
x = 3;
if (NS1::x != 12) print("failed #7");

// Note that inside 'x1' & 'x2' below, a lexical reference to 'a'
// will bind to the static variable, not the instance variable.
class T {
    static var a;
    var a;
    function x1() { this.a = T.a++; }
    function x2() { return a; }
};

T.a = 1;

var s1:T, s2:T;
s1 = new T()
s1.x1();
s2 = new T()
s2.x1();
if (s1.a != 1) print("failed #7");
if (s2.a != 2) print("failed #8");
if (T.a != 3) print("failed #9");
if (s2.x2() != 3) print("failed #10");


// bind the instance to it's method for later invocation
var m = s1.x1;
m()
if (s1.a != 3) print("failed #11");


/*
// make sure only one static var can be defined
test = false;
try {
    eval("static var t1; static var t1;");
} 
catch (ex) {
    // spec. actually seems to require 'DefinitionError'
    // but i'm not sure if that's going to be an actual
    // ECMA error...
    test = (ex.name == "Error");
}
if (!test) print("failed #12");
*/

// check the typeof some things

if (typeof t != "Function") print("failed #13");
if (typeof s1 != "T") print("failed #14");
if (typeof c != "C") print("failed #15");
if (typeof test != "boolean") print("failed #16");

print("passed");



