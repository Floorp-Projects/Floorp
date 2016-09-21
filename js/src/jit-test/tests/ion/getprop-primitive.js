
var testSet1 = [1, "", Symbol("a"), true];
var testSet2 = [1, "", Symbol("a"), true, { bar: 5 }];

Number.prototype.bar = 1;
String.prototype.bar = 2;
Symbol.prototype.bar = 3;
Boolean.prototype.bar = 4;

function assertEqIf(prev, curr, expected) {
    // Branch pruning absolutely want to get rid of the next branch
    // which causes bailouts, so we forbid inlining of this function.
    with({}){}
    if (prev) {
        assertEq(curr, expected);
        return false;
    }
    return true;
}

var f;
var template = function (set) {
    var lastX = 0, x = 0, i = 0, y = 0;
    var cont = true;
    while (cont) { // OSR here.
        for (var i = 0; i < set.length; i++) {
            x = x + (inIon() ? 1 : 0);
            if (set[i].placeholder != set[(i + 1) % set.length].placeholder)
                y += 1;
        }

        // If we bailout in the inner loop, then x will have a smaller value
        // than the number of iterations.
        cont = assertEqIf(lastX > 0, x, set.length);
        lastX = x;
        x = 0;
    }
    return y;
}

// Set 1, Non existing properties.
f = eval(template.toSource().replace(".placeholder", ".foo"));
f(testSet1);

// Set 2, Non existing properties.
f = eval(template.toSource().replace(".placeholder", ".foo"));
f(testSet2);

// Set 1, Existing properties.
f = eval(template.toSource().replace(".placeholder", ".bar"));
f(testSet1);

// Set 2, Existing properties.
f = eval(template.toSource().replace(".placeholder", ".bar"));
f(testSet2);
