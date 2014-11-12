var C = {};
var B = new Proxy(C, {});
var A = Object.create(B);
B.x = 1;
assertEq(C.x, 1);
