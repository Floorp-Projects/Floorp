setJitCompilerOption("ion.warmup.trigger", 30);
var max = 40;

// This test case verify that even if we do some scalar replacement, we keep a
// correct computation of Float32 maths.  In this case, when the object is not
// escaped, the "store" instruction are preventing any float32 optimization to
// kick-in.  After Scalar Replacement, the store is removed, and the Float32
// optimizations can avoid Double coercions.
function escape_object(o) {
    if (o.e) {
        print(o);
    }
}

var func = null;
var check_object_argument_func = function (i, res) {
    with ({}) { /* trun off the jit for this function, do not inline */ };
    if (i == max - 1)
        return funname.arguments[1].d;
    return res;
};

var test_object_ref_check = eval(uneval(check_object_argument_func).replace("funname", "test_object_ref"));
function test_object_ref(x, tmp) {
    tmp = {
        a: Math.fround(Math.pow(2 * x / max, 0)),
        b: Math.fround(Math.pow(2 * x / max, 25)),
        c: Math.fround(Math.pow(2 * x / max, 50)),
        d: 0
    };

    tmp.d = tmp.a + tmp.b;
    assertFloat32(tmp.d, false);
    escape_object(tmp);
    return test_object_ref_check(x, Math.fround(tmp.c + Math.fround(tmp.d)));
}

var test_object_check = eval(uneval(check_object_argument_func).replace("funname", "test_object"));
function test_object(x, tmp) {
    tmp = {
        a: Math.fround(Math.pow(2 * x / max, 0)),
        b: Math.fround(Math.pow(2 * x / max, 25)),
        c: Math.fround(Math.pow(2 * x / max, 50)),
        d: 0
    };

    tmp.d = tmp.a + tmp.b;
    assertFloat32(tmp.d, false);
    return test_object_check(x, Math.fround(tmp.c + Math.fround(tmp.d)));
}

// Same test with Arrays.
function escape_array(o) {
    if (o.length == 0) {
        print(o);
    }
}

var check_array_argument_func = function (i, res) {
    with ({}) { /* trun off the jit for this function, do not inline */ };
    if (i == max - 1) {
        return funname.arguments[1][3];
    }
    return res;
};

var test_array_ref_check = eval(uneval(check_array_argument_func).replace("funname", "test_array_ref"));
function test_array_ref(x, tmp) {
    tmp = [
        Math.fround(Math.pow(2 * x / max, 0)),
        Math.fround(Math.pow(2 * x / max, 25)),
        Math.fround(Math.pow(2 * x / max, 50)),
        0
    ];
    tmp[3] = tmp[0] + tmp[1];
    assertFloat32(tmp[3], false);
    escape_array(tmp);
    return test_array_ref_check(x, Math.fround(tmp[2] + Math.fround(tmp[3])));
}

var test_array_check = eval(uneval(check_array_argument_func).replace("funname", "test_array"));
function test_array(x, tmp) {
    tmp = [
        Math.fround(Math.pow(2 * x / max, 0)),
        Math.fround(Math.pow(2 * x / max, 25)),
        Math.fround(Math.pow(2 * x / max, 50)),
        0
    ];
    tmp[3] = tmp[0] + tmp[1];
    assertFloat32(tmp[3], false);
    return test_array_check(x, Math.fround(tmp[2] + Math.fround(tmp[3])));
}


for (var i = 0; i < max; i++) {
    assertEq(test_object_ref(i, undefined), test_object(i, undefined));
    assertEq(test_array_ref(i, undefined), test_array(i, undefined));
}
