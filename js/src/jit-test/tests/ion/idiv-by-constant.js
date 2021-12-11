function int_seq(count) {
    var arr = [];
    var x = 0xfac83126;
    while (count--) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        arr.push(x | 0);
    }
    return arr;
}

function test(name, asm, ion, int) {
    let count = 10000;
    let seq = int_seq(count);
    for (let x of seq) {
        let rint = int(x);
        let rasm = asm(x);
        let rion = ion(x);
        // console.log(name, x, rint, rasm, rion);
        assertEq(rasm, rint);
        assertEq(rion, rint);
    }
}

var asmdiv2 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x|0) / 2)|0;
        return z|0;
    }
    return f;
})()

var plaindiv2 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x|0) / 2)|0;
    return z|0;
}

var interpdiv2 = function(x) {
    with({}){};
    x = x|0;
    var z = 0;
    z = ((x|0) / 2)|0;
    return z|0;
}

test("div2", asmdiv2, plaindiv2, interpdiv2);

var asmdiv3 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x|0) / 3)|0;
        return z|0;
    }
    return f;
})()

var plaindiv3 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x|0) / 3)|0;
    return z|0;
}

var interpdiv3 = function(x) {
    with({}){};
    x = x|0;
    var z = 0;
    z = ((x|0) / 3)|0;
    return z|0;
}

test("div3", asmdiv3, plaindiv3, interpdiv3);

var asmdiv7 = (function(m) {
    "use asm"
    function f(x) {
        x = x|0;
        var z = 0;
        z = ((x|0) / 7)|0;
        return z|0;
    }
    return f;
})()

var plaindiv7 = function(x) {
    x = x|0;
    var z = 0;
    z = ((x|0) / 7)|0;
    return z|0;
}

var interpdiv7 = function(x) {
    with({}){};
    x = x|0;
    var z = 0;
    z = ((x|0) / 7)|0;
    return z|0;
}

test("div7", asmdiv7, plaindiv7, interpdiv7);
