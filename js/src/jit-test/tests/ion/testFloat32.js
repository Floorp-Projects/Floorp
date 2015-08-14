// This test checks that we are able to optimize float32 inputs.  As
// GetElementIC (float32 array accesses) output is not specialized with Float32
// output types, we should not force inline caches.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

// Fuzz tests
(function(){
    //
    (function(){
        var g = {};
        x = new Float32Array()
        Function('g', "g.o = x[1]")(g);
    })();
    //
    (function() {
        var g = new Float32Array(16);
        var h = new Float64Array(16);
        var farrays = [ g, h ];
        for (aridx = 0; aridx < farrays.length; ++aridx) {
            ar  = farrays[aridx];
            !(ar[ar.length-2] == (NaN / Infinity)[ar.length-2])
        }
    })();
    //
    (function () {
        var v = new Float32Array(32);
        for (var i = 0; i < v.length; ++i)
        v[i] = i;
    var t = (false  );
    for (var i = 0; i < i .length; ++i)
        t += v[i];
    })();
    //
    (function() {
        x = y = {};
        z = new Float32Array(6)
        for (c in this) {
            Array.prototype.unshift.call(x, new ArrayBuffer())
        }
        Array.prototype.sort.call(x, (function (j) {
            y.s = z[2]
        }))
    })();
    //
    (function() {
        // bug 1134298
        for (var k = 0; k < 1; k++) {
            Math.fround(Math.ceil(Math.fround(Math.acos(3.0))))
        }
    })();
})();
//
// ION TESTS
//
// The assertFloat32 function is deactivated in --ion-eager mode, as the first time, the function Math.fround
// would be guarded against modifications (typeguard on Math and then on fround). In this case, Math.fround is
// not inlined and the compiler will consider the return value to be a double, not a float32, making the
// assertions fail. Note that as assertFloat32 is declared unsafe for fuzzing, this can't happen in fuzzed code.
//
// To be able to test it, we still need ion compilation though. A nice solution
// is to manually lower the ion warm-up trigger.
setJitCompilerOption("ion.warmup.trigger", 50);

function test(f) {
    f32[0] = .5;
    for(var n = 110; n; n--)
        f();
}

var f32 = new Float32Array(4);
var f64 = new Float64Array(4);

function acceptAdd() {
    var use = f32[0] + 1;
    assertFloat32(use, true);
    f32[0] = use;
}
test(acceptAdd);

function acceptAddSeveral() {
    var sum1 = f32[0] + 0.5;
    var sum2 = f32[0] + 0.5;
    f32[0] = sum1;
    f32[0] = sum2;
    assertFloat32(sum1, true);
    assertFloat32(sum2, true);
}
test(acceptAddSeveral);

function acceptAddVar() {
    var x = f32[0] + 1;
    f32[0] = x;
    f32[1] = x;
    assertFloat32(x, true);
}
test(acceptAddVar);

function refuseAddCst() {
    var x = f32[0] + 1234567890; // this constant can't be precisely represented as a float32
    f32[0] = x;
    assertFloat32(x, false);
}
test(refuseAddCst);

function refuseAddVar() {
    var x = f32[0] + 1;
    f32[0] = x;
    f32[1] = x;
    f64[1] = x; // non consumer
    assertFloat32(x, false);
}
test(refuseAddVar);

function refuseAddStore64() {
    var x = f32[0] + 1;
    f64[0] = x; // non consumer
    f32[0] = f64[0];
    assertFloat32(x, false);
}
test(refuseAddStore64);

function refuseAddStoreObj() {
    var o = {}
    var x = f32[0] + 1;
    o.x = x; // non consumer
    f32[0] = o['x'];
    assertFloat32(x, false);
}
test(refuseAddStoreObj);

function refuseAddSeveral() {
    var sum = (f32[0] + 2) - 1; // second addition is not a consumer
    f32[0] = sum;
    assertFloat32(sum, false);
}
test(refuseAddSeveral);

function refuseAddFunctionCall() {
    function plusOne(x) { return Math.cos(x+1)*13.37; }
    var res = plusOne(f32[0]); // func call is not a consumer
    f32[0] = res;
    assertFloat32(res, false);
}
test(refuseAddFunctionCall);

function acceptSqrt() {
    var res = Math.sqrt(f32[0]);
    assertFloat32(res, true);
    f32[0] = res;
}
test(acceptSqrt);

function refuseSqrt() {
    var res = Math.sqrt(f32[0]);
    assertFloat32(res, false);
    f32[0] = res + 1;
}
test(refuseSqrt);

function acceptMin() {
    var res = Math.min(f32[0], f32[1]);
    assertFloat32(res, true);
    f64[0] = res;
}
test(acceptMin);

// In theory, we could do it, as Math.min/max actually behave as a Phi (it's a
// float32 producer iff its inputs are producers, it's a consumer iff its uses
// are consumers). In practice, this would involve some overhead for big chains
// of min/max.
function refuseMinAdd() {
    var res = Math.min(f32[0], f32[1]) + f32[2];
    assertFloat32(res, false);
    f32[3] = res;
}
test(refuseMinAdd);

function acceptSeveralMinMax() {
    var x = Math.min(f32[0], f32[1]);
    var y = Math.max(f32[2], f32[3]);
    var res = Math.min(x, y);
    assertFloat32(res, true);
    f64[0] = res;
}
test(acceptSeveralMinMax);

function acceptSeveralMinMax2() {
    var res = Math.min(f32[0], f32[1], f32[2], f32[3]);
    assertFloat32(res, true);
    f64[0] = res;
}
test(acceptSeveralMinMax2);

function partialMinMax() {
    var x = Math.min(f32[0], f32[1]);
    var y = Math.min(f64[0], f32[1]);
    var res  = Math.min(x, y);
    assertFloat32(x, true);
    assertFloat32(y, false);
    assertFloat32(res, false);
    f64[0] = res;
}
test(partialMinMax);

function refuseSeveralMinMax() {
    var res = Math.min(f32[0], f32[1] + f32[2], f32[2], f32[3]);
    assertFloat32(res, false);
    f64[0] = res;
}
test(refuseSeveralMinMax);

function refuseMin() {
    var res = Math.min(f32[0], 42.13 + f32[1]);
    assertFloat32(res, false);
    f64[0] = res;
}
test(refuseMin);

function acceptMax() {
    var res = Math.max(f32[0], f32[1]);
    assertFloat32(res, true);
    f64[0] = res;
}
test(acceptMax);

function refuseMax() {
    var res = Math.max(f32[0], 42.13 + f32[1]);
    assertFloat32(res, false);
    f64[0] = res;
}
test(refuseMax);

function acceptAbs() {
    var res = Math.abs(f32[0]);
    assertFloat32(res, true);
    f32[0] = res;
}
test(acceptAbs);

function refuseAbs() {
    var res = Math.abs(f32[0]);
    assertFloat32(res, false);
    f64[0] = res + 1;
}
test(refuseAbs);

function acceptFilterTypeSet() {
    var res = f32[0];
    if (!res) {
    } else {
        f32[0] = res;
        assertFloat32(res, true);
    }
}
test(acceptFilterTypeSet);

function acceptFilterTypeSet2() {
    var res = f32[0];
    if (!res) {
    } else {
        var res1 = Math.abs(res);
        f32[0] = res1;
        assertFloat32(res1, true);
    }
}
test(acceptFilterTypeSet2);

function refuseFilterTypeSet() {
    var res = f32[0];
    if (!res) {
    } else {
        var res1 = Math.abs(res);
        f64[0] = res1 + 1;
        assertFloat32(res1, false);
    }
}
test(refuseFilterTypeSet);

function refuseTrigo() {
    var res = Math.cos(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    var res = Math.sin(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    var res = Math.tan(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    var res = Math.acos(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    var res = Math.asin(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.atan(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);
}
test(refuseTrigo);

function refuseMath() {
    var res = Math.log10(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.log2(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.log1p(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.expm1(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.cosh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.sinh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.tanh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.acosh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.asinh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.atanh(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.cbrt(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.sign(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);

    res = Math.trunc(f32[0]);
    f32[0] = res;
    assertFloat32(res, false);
}
test(refuseMath);

function refuseLoop() {
    var res = f32[0],
        n = 10;
    while (n--) {
        res = res + 1; // this loop is equivalent to several additions => second addition is not a consumer
        assertFloat32(res, false);
    }
    assertFloat32(res, false);
    f32[0] = res;
}
test(refuseLoop);

function acceptLoop() {
    var res = f32[0],
        n = 10;
    while (n--) {
        var sum = res + 1;
        res = Math.fround(sum);
        assertFloat32(sum, true);
    }
    assertFloat32(res, true);
    f32[0] = res;
}
test(acceptLoop);

function alternateCond(n) {
    var x = f32[0];
    if (n > 0) {
        var s1 = x + 1;
        f32[0] = s1;
        assertFloat32(s1, true);
    } else {
        var s2 = x + 1;
        f64[0] = s2; // non consumer
        assertFloat32(s2, false);
    }
}
(function() {
    f32[0] = 0;
    for (var n = 110; n; n--) {
        alternateCond(n % 2);
    }
})();

function phiTest(n) {
    var x = (f32[0]);
    var y = n;
    if (n > 0) {
        x = x + 2;
        assertFloat32(x, true);
    } else {
        if (n < -10) {
            x = Math.fround(Math.sqrt(y));
            assertFloat32(x, true);
        } else {
            x = x - 1;
            assertFloat32(x, true);
        }
    }
    assertFloat32(x, true);
    f32[0] = x;
}
(function() {
    f32[0] = 0;
    for (var n = 100; n; n--) {
        phiTest( ((n % 3) - 1) * 15 );
    }
})();

function mixedPhiTest(n) {
    var x = (f32[0]);
    var y = n;
    if (n > 0) {
        x = x + 2; // non consumer because of (1)
        assertFloat32(x, false);
    } else {
        if (n < -10) {
            x = Math.fround(Math.sqrt(y)); // new producer
            assertFloat32(x, true);
        } else {
            x = x - 1; // non consumer because of (1)
            assertFloat32(x, false);
        }
    }
    assertFloat32(x, false);
    x = x + 1; // (1) non consumer
    f32[0] = x;
}
(function() {
    f32[0] = 0;
    for (var n = 100; n; n--) {
        mixedPhiTest( ((n % 3) - 1) * 15 );
    }
})();

function phiTest2(n) {
    var x = f32[0];
    while (n >= 0) {
        x = Math.fround(Math.fround(x) + 1);
        assertFloat32(x, true);
        if (n < 10) {
            x = f32[0] + 1;
            assertFloat32(x, true);
        }
        n = n - 1;
    }
}
(function(){
    f32[0] = 0;
    for (var n = 100; n > 10; n--) {
        phiTest2(n);
    }
})();
