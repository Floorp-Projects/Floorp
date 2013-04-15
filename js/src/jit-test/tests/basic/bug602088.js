// |jit-test| error: TypeError
/* vim: set ts=8 sts=4 et sw=4 tw=99: */

var p = Proxy.createFunction({}, function(x, y) { undefined.x(); });
print(new p(1, 2));

