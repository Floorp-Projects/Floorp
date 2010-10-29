// |jit-test| error: TypeError
/* vim: set ts=4 sw=4 tw=99 et: */

var p = Proxy.createFunction({}, function(x, y) { undefined.x(); });
print(new p(1, 2));

