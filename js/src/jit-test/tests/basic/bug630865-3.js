var a = [];
function next() {
    var x = {};
    a.push(x);
    return x;
}
Object.defineProperty(Function.prototype, 'prototype', {get: next});
var b = [];
for (var i = 0; i < HOTLOOP + 2; i++)
    b[i] = new Function.prototype;
for (var i = 0; i < HOTLOOP + 2; i++)
    assertEq(Object.getPrototypeOf(b[i]), a[i]);
