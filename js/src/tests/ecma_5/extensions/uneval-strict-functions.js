/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Check that strict mode functions get decompiled properly. */
function lenient() { return typeof this == "object"; }
assertEq(eval(uneval(lenient) + "lenient;")(), true);

function strict()  { 'use strict'; return typeof this == "undefined"; }
print(uneval(strict));
assertEq(eval(uneval(strict) + "strict;")(), true);

function lenient_outer() {
    function lenient_inner() {
        return typeof this == "object";
    }
    return lenient_inner;
}
assertEq(eval(uneval(lenient_outer()) + "lenient_inner;")(), true);

function strict_outer() {
    "use strict";
    function strict_inner() {
        return typeof this == "undefined";
    }
    return strict_inner;
}
assertEq(eval(uneval(strict_outer()) + "strict_inner;")(), true);

function lenient_outer_closure() {
    return function lenient_inner_closure() { 
               return typeof this == "object"; 
           };
}
assertEq(eval(uneval(lenient_outer_closure()))(), true);

function strict_outer_closure() {
    "use strict";
    return function strict_inner_closure() { 
               return typeof this == "undefined"; 
           };
}
assertEq(eval(uneval(strict_outer_closure()))(), true);

function lenient_outer_expr() {
    return function lenient_inner_expr() (typeof this == "object");
}
assertEq(eval(uneval(lenient_outer_expr()))(), true);

/*
 * This doesn't work, because we have no way to include strict mode
 * directives in expression closures.
 *
 *   function strict_outer_expr() {
 *       return function strict_inner_expr() (typeof this == "undefined");
 *   }
 *   assertEq(eval(uneval(strict_outer_expr()))(), true);
 */

reportCompare(true, true);
