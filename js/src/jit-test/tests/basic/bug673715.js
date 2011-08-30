function g() {
    "use strict";
    for (var i = 0; i < 50; i++)
        arguments[0];
    eval("");
}
function f() {
    g.call(arguments);
}
f();
