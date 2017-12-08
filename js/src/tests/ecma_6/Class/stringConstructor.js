class A {
    "constructor"() { return {}; }
}
assertEq(new A() instanceof A, false);

class B extends class { } {
    "constructor"() { return {}; }
}
assertEq(new B() instanceof B, false);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
