function warmup(fun, input, expected) {
    assertEq(input.length, expected.length);
    for (var i = 0; i < 30; i++) {
        for (var j = 0; j < input.length; j++) {
            lhs = input[j][0];
            rhs = input[j][1];
            assertEq(fun(lhs,rhs), expected[j]);
        }
    }
}

var strictCompare = function(a,b) { return a === b; }
warmup(strictCompare, [[1,1], [3,3], [3,strictCompare],[strictCompare, {}], [3.2, 1],
                       [0, -0]],
                       [true, true,  false, false, false,
                       true]);