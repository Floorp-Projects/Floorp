/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function f(s) {
    eval(s);
    return function(a) {
        eval(a);
        let (c = 3) {
            eval(s);
            return b;
        };
    };
}

var b = 1;
var g1 = f('');
var g2 = f('');

/* Call the lambda once, caching a reference to the global b. */
g1('');

/*
 * If this call sees the above cache entry, then it will erroneously use the
 * global b.
 */
assertEq(g2('var b=2'), 2);

reportCompare(true, true);
