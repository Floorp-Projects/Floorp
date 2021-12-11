var counter = 0;
function inc() { return counter++ }
var imp = { inc:inc };

function FFI1(stdlib, foreign) {
    "use asm";

    var inc = foreign.inc;

    function g() {
      return inc()|0
    }

    return g
}

function FFI2(stdlib, foreign) {
    "use asm";

    var inc=foreign.inc;

    function g() {
      inc()
    }

    return g
}


var f = FFI2(this, imp);     // produces AOT-compiled version
f()
assertEq(counter, 1);

var f = FFI1(this, imp);     // produces AOT-compiled version

assertEq(f(), 1);
assertEq(counter, 2);
assertEq(f(), 2);
assertEq(counter, 3);
