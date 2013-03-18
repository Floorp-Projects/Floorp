// 'this' in a toplevel arrow is the global object.

var f = () => this;
assertEq(f(), this);
assertEq({f: f}.f(), this);
