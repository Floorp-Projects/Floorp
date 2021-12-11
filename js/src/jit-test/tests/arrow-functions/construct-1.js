// Arrow functions are not constructors.

load(libdir + "asserts.js");

var f = a => { this.a = a; };
assertThrowsInstanceOf(() => new f, TypeError);
assertThrowsInstanceOf(() => new f(1, 2), TypeError);
