
function g() { }
function strict() {
    "use strict";
}

let bound = g.bind();
let arrow = x => 0;

async function fn_async() { }
function * fn_generator() { }

let o = {
    mtd() {},
    get x() {},
    set x(v) {},
};

class Base { }
class Derived extends Base { }

function asm_mod() {
    "use asm";
    function mtd() {}
    return { mtd: mtd }
}

let asm_fun = (new asm_mod).mtd;

let builtin_selfhost = [].sort;
let builtin_native = Math.sin;

let dot_caller = Object.getOwnPropertyDescriptor(Function.__proto__,
                                                 "caller").get;

// Returns true if fn.caller is allowed
function check(fn) {
    try {
        (function() {
            fn.caller;
        })();
    }
    catch (e) {
        assertEq(e instanceof TypeError, true);
        return false;
    }
    return true;
}

// Normal sloppy functions are allowed, even if they also are intended as
// asm.js.
assertEq(check(g), true);
assertEq(check(asm_mod), true);
assertEq(check(asm_fun), true);

// Most others are not
assertEq(check(strict), false);
assertEq(check(bound), false);
assertEq(check(arrow), false);
assertEq(check(fn_async), false);
assertEq(check(fn_generator), false);
assertEq(check(o.mtd), false)
assertEq(check(Object.getOwnPropertyDescriptor(o, "x").get), false)
assertEq(check(Object.getOwnPropertyDescriptor(o, "x").set), false)
assertEq(check(Base), false);
assertEq(check(Derived), false);
assertEq(check(builtin_selfhost), false);
assertEq(check(builtin_native), false);
assertEq(check(dot_caller), false);

// Have a native invoke .caller on our behalf.
function foo() {
    function inner() {
        return callFunctionFromNativeFrame(dot_caller.bind(inner))
    }
    return inner();
}
assertEq(foo, foo());
