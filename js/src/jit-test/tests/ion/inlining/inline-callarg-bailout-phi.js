function add(x, y) {
    if (x & 0x1)
        return x + y;
    else
        return y + x;
}

function runBinop(binop, lhs, rhs) {
    return binop(lhs, rhs);
}

//dis(run_binop);

// Get the add function to compile.
var accum = 0;
for (var i = 0; i < 1000; ++i)
    accum += add(1, 1);
print(accum);

// Get the runBinop function to compile and inline the add function.
var accum = 0;
for (var i = 0; i < 50; ++i)
    accum = runBinop(add, i, i);
print(accum);

var t30 = 1 << 30;
var t31 = t30 + t30;
var result = runBinop(add, t31, t31);
assertEq(result, Math.pow(2, 32));
