// Let's calculate zero in some elaborate ways.
function testFancyZeroOffset(fancyZero, memType = 'i32') {
    try {
        const { mem } = wasmEvalText(`(module
            (memory (export "mem") ${memType} 1)
            (data (offset ${fancyZero}) "hi")
        )`).exports;
        const str = String.fromCharCode(...new Uint8Array(mem.buffer).slice(0, 2));
        assertEq(str, 'hi');
    } catch (e) {
        const { getOffset } = wasmEvalText(`(module
            (func (export "getOffset") (result ${memType})
                ${fancyZero}
            )
        )`).exports;
        console.log('Computed offset:', getOffset());
        throw e;
    }
}

// Do plus one minus one a thousand times
testFancyZeroOffset('i32.const 0 ' + (
    '(i32.add (i32.const 1)) '
    + '(i32.sub (i32.const 1)) '
).repeat(1000));

// Do some jank fibonacci
{
    let fib = '(i32.const 1)\n'
    let a = 1; let b = 1; let next;
    for (let i = 0; i < 45; i++) {
        fib += `(i32.const ${a})\n`;
        fib += '(i32.add)\n';
        next = a + b;
        a = b;
        b = next;
    }
    fib += `(i32.sub (i32.const ${next}))\n`;
    testFancyZeroOffset(fib);
}

// Run the collatz conjecture as long as possible
{
    let val = 837799; // should reach 1 in 524 steps
    let expr = `(i32.const ${val})\n`;
    while (val != 1) {
        if (val % 2 == 0) {
            expr += `(i32.sub (i32.const ${val / 2}))\n`; // we can't divide in constant expressions lol
            val /= 2;
        } else {
            expr += `(i32.mul (i32.const 3))\n`;
            expr += `(i32.add (i32.const 1))\n`;
            val = val * 3 + 1;
        }
    }
    expr += `(i32.sub (i32.const 1))\n`;
    testFancyZeroOffset(expr);
}

// The collatz conjecture would be even more fun with 64-bit numbers...
if (wasmMemory64Enabled()) {
    let val = 1899148184679; // should reach 1 in 1411 steps
    let expr = `(i64.const ${val})\n`;
    while (val != 1) {
        if (val % 2 == 0) {
            expr += `(i64.sub (i64.const ${val / 2}))\n`; // we can't divide in constant expressions lol
            val /= 2;
        } else {
            expr += `(i64.mul (i64.const 3))\n`;
            expr += `(i64.add (i64.const 1))\n`;
            val = val * 3 + 1;
        }
    }
    expr += `(i64.sub (i64.const 1))\n`;
    testFancyZeroOffset(expr, 'i64');
}
