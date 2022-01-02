// |jit-test| error:InternalError

var a = [];
var f = a.forEach.bind(a);
a.push(f);
f(f);
