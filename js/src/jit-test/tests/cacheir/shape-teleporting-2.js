function A(name) { this.name = name; }
function B() { }
function C() { }

B.prototype = A0 = new A("0");
C.prototype = B0 = new B();

var A1 = new A("1");
var A2 = new A("2");

var B1 = new B();
var B2 = new B();

var C1 = new C();
var C2 = new C();

// Object <-+- A0 <-+- B0 <-+
//          |       |       |
//          +- A1   +- B1   +- C1
//          |       |       |
//          +- A2   +- B2   +- C2

Object.setPrototypeOf(C1, B1);
Object.setPrototypeOf(C2, B2);

Object.setPrototypeOf(B1, A1);
Object.setPrototypeOf(B2, A2);

// Object <-+- A0 <--- B0
//          |
//          +- A1 <--- B1 <--- C1
//          |
//          +- A2 <--- B2 <--- C2


function getName(o) { return o.name; }

// Warm up JIT
for (var i = 0; i < 100; i++) {
    getName(C1);
}

assertEq(B1.name, "1");
assertEq(B2.name, "2");

assertEq(getName(B1), "1");
assertEq(getName(B2), "2");
