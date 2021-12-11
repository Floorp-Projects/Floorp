load(libdir + "asserts.js");

function*g(){ };
o = g();
o.next();
function TestException() {};
assertThrowsInstanceOf(() => o.throw(new TestException()), TestException);
