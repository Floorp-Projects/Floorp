function f() {
    "use strict";
}
g = new Proxy(f, {});
Object.defineProperty(g, "arguments", {set: function(){}});
