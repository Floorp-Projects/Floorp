
Array.prototype.__proto__ = Function.prototype;
var x = [1,2,3];
x[0];

[].__proto__.foo = true;
eval("[]");
