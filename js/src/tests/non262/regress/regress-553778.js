/*
 * This should compile without triggering the following assertion:
 * 
 * Assertion failure: cg->fun->u.i.skipmin <= skip, at ../jsemit.cpp:2346
 */

function f() {
    function g() {
        function h() {
            g; x;
        }
        var [x] = [];
    }
}

reportCompare(true, true);
