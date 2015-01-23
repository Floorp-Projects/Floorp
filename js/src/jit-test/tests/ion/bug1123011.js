
var global = this;
function f() {
    return eval("'use strict'; this;");
}
for (var j = 0; j < 5; ++j) {
    assertEq(f(), global);
}
