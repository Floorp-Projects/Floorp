
"use strict";

function loop(a) {
    a = arguments.length;
    var result = 0;
    for (var i = 0; i < 5000; i++) {
        result += a;
    }
    return result;
}

assertEq(loop(11), 5000);
