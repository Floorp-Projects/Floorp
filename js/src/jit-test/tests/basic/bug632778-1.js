load(libdir + "asserts.js");
function f() {
    "use strict";
}
g = wrap(f);
assertThrowsInstanceOf(function () { Object.defineProperty(g, "arguments", {set: function(){}}); }, TypeError);

