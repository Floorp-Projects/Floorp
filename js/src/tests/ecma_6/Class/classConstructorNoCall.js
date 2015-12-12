// Class constructors don't have a [[Call]]
class Foo {
    constructor() { }
}

assertThrowsInstanceOf(Foo, TypeError);

class Bar extends Foo {
    constructor() { }
}

assertThrowsInstanceOf(Bar, TypeError);

assertThrowsInstanceOf(class { constructor() { } }, TypeError);
assertThrowsInstanceOf(class extends Foo { constructor() { } }, TypeError);

assertThrowsInstanceOf(class foo { constructor() { } }, TypeError);
assertThrowsInstanceOf(class foo extends Foo { constructor() { } }, TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
