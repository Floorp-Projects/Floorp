// The constructor specified should get called, regardless of order, or
// other distractions

var test = `

var called = false;
class a { constructor(x) { assertEq(x, 4); called = true } }
new a(4);
assertEq(called, true);

called = false;
class b { constructor() { called = true } method() { } }
new b();
assertEq(called, true);

called = false;
class c { method() { } constructor() { called = true; } }
new c();
assertEq(called, true);

called = false;
class d { [\"constructor\"]() { throw new Error(\"NO\"); } constructor() { called = true; } }
new d();
assertEq(called, true);
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
