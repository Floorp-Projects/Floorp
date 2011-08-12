
/* Inheritance of shadowed function properties from Object.prototype. */

delete Function.prototype.toString;
assertEq(Function.prototype.toString, Object.prototype.toString);
