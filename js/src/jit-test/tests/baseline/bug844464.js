// |jit-test| error: TypeError
Object = function () {};
var p = Proxy.create({});
Object.prototype.__proto__ = p;
function newFunc(x) { new Function(x)(); };
newFunc('\
function f(v, value) {\
    "failed: " + v + " " + value\
}\
f({}, false);\
f(Object.prototype, false);\
');
