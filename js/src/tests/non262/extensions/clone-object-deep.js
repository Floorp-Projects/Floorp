// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function test() {
    var check = clone_object_check;

    // Invoke with the simple parameter to compile the function before doing
    // deep clone, on --ion-eager case, to avoid timeout.
    check({x: null, y: undefined});

    // Try cloning a deep object. Don't fail with "too much recursion".
    var b = {};
    var current = b;
    for (var i = 0; i < 10000; i++) {
        var next = {};
        current['x' + i] = next;
        current = next;
    }
    check(b, "deepObject");  // takes 2 seconds :-\
}

test();
reportCompare(0, 0, 'ok');
