function func1() { return "123" }
function func2(a,b,c,d,e) { return "123" }
var imp = { func1:func1, func2:func2 };

function FFI1(stdlib, foreign) {
    "use asm";

    var func1 = foreign.func1;
    var func2 = foreign.func2;

    function g() {
      return func1()|0
    }

    function h() {
      return func2()|0
    }

    return {g:g, h:h};
}

var f = FFI1(this, imp);     // produces AOT-compiled version

assertEq(f.g(), 123);
assertEq(f.g(), 123);

assertEq(f.h(), 123);
assertEq(f.h(), 123);
