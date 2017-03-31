function inner_env() {
    let result = [];

    let x = 0;
    result.push(() => x);

    var tmp = [1,2,3];
    for (let y in tmp)
        result.push(() => tmp[y])

    for (let z = 4; z < 7; z++)
        result.push(() => z)

    return result;
}

function outer_env() {
    let result = inner_env();

    var tmp = [7,8,9];
    for (let x in tmp)
        result.push(() => tmp[x])

    return result;
}

function check_result(result, expectedLen) {
    assertEq(result.length, expectedLen);

    for (var i = 0; i < expectedLen; ++i)
        assertEq(result[i], i);
}

// Wipeout jitcode
bailout();
gc(); gc();

// Test lexical environment bailouts
for (var i = 0; i < 100; ++i)
{
    bailAfter(i);

    var result = inner_env().map(fn => fn());
    check_result(result, 7);
}

// Test inlined lexical environment bailouts
for (var i = 0; i < 100; ++i)
{
    bailAfter(i);

    var result = outer_env().map(fn => fn());
    check_result(result, 10);
}
