load(libdir + 'simd.js');

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);

var max = 40, pivot = 35;

var i32x4 = SIMD.Int32x4;
var f32x4 = SIMD.Float32x4;
var i32x4Add = SIMD.Int32x4.add;

var FakeSIMDType = function (o) { this.x = o.x; this.y = o.y; this.z = o.z; this.w = o.w; };
if (this.hasOwnProperty("TypedObject")) {
  var TO = TypedObject;
  FakeSIMDType = new TO.StructType({ x: TO.int32, y: TO.int32, z: TO.int32, w: TO.int32 });
}

function simdunbox_bail_undef(i, lhs, rhs) {
  return i32x4Add(lhs, rhs);
}

function simdunbox_bail_object(i, lhs, rhs) {
  return i32x4Add(lhs, rhs);
}

function simdunbox_bail_typeobj(i, lhs, rhs) {
  return i32x4Add(lhs, rhs);
}

function simdunbox_bail_badsimd(i, lhs, rhs) {
  return i32x4Add(lhs, rhs);
}

var arr_undef = [ i32x4(0, 1, 1, 2), i32x4(1, 1, 2, 3) ];
var fail_undef = 0;
var arr_object = [ i32x4(0, 1, 1, 2), i32x4(1, 1, 2, 3) ];
var fail_object = 0;
var arr_typeobj = [ i32x4(0, 1, 1, 2), i32x4(1, 1, 2, 3) ];
var fail_typeobj = 0;
var arr_badsimd = [ i32x4(0, 1, 1, 2), i32x4(1, 1, 2, 3) ];
var fail_badsimd = 0;
for (var i = 0; i < max; i++) {
  try {
    arr_undef[i + 2] = simdunbox_bail_undef(i, arr_undef[i], arr_undef[i + 1]);
  } catch (x) {
    arr_undef[i + 2] = arr_undef[i - 1];
    fail_undef++;
  }

  try {
    arr_object[i + 2] = simdunbox_bail_object(i, arr_object[i], arr_object[i + 1]);
  } catch (x) {
    arr_object[i + 2] = arr_object[i - 1];
    fail_object++;
  }

  try {
    arr_typeobj[i + 2] = simdunbox_bail_typeobj(i, arr_typeobj[i], arr_typeobj[i + 1]);
  } catch (x) {
    arr_typeobj[i + 2] = arr_typeobj[i - 1];
    fail_typeobj++;
  }

  try {
    arr_badsimd[i + 2] = simdunbox_bail_badsimd(i, arr_badsimd[i], arr_badsimd[i + 1]);
  } catch (x) {
    arr_badsimd[i + 2] = arr_badsimd[i - 1];
    fail_badsimd++;
  }

  if (i + 2 == pivot) {
    arr_undef[pivot] = undefined;
    arr_object[pivot] = { x: 0, y: 1, z: 2, w: 3 };
    arr_typeobj[pivot] = new FakeSIMDType({ x: 0, y: 1, z: 2, w: 3 });
    arr_badsimd[pivot] = f32x4(0, 1, 2, 3);
  }
}

assertEq(fail_undef, 2);
assertEq(fail_object, 2);
assertEq(fail_typeobj, 2);
assertEq(fail_badsimd, 2);

// Assert that all SIMD values are correct.
function assertEqX4(real, expected, assertFunc) {
    if (typeof assertFunc === 'undefined')
        assertFunc = assertEq;

    assertFunc(real.x, expected[0]);
    assertFunc(real.y, expected[1]);
    assertFunc(real.z, expected[2]);
    assertFunc(real.w, expected[3]);
}

var fib = [0, 1];
for (i = 0; i < max + 5; i++)
  fib[i+2] = (fib[i] + fib[i+1]) | 0;

for (i = 0; i < max; i++) {
  if (i == pivot)
    continue;
  var ref = fib.slice(i < pivot ? i : i - 3);
  assertEqX4(arr_undef[i], ref);
  assertEqX4(arr_object[i], ref);
  assertEqX4(arr_typeobj[i], ref);
  assertEqX4(arr_badsimd[i], ref);
}

// Check that unbox operations aren't removed
(function() {

    function add(i, v, w) {
        if (i % 2 == 0) {
            SIMD.Int32x4.add(v, w);
        } else {
            SIMD.Float32x4.add(v, w);
        }
    }

    var i = 0;
    var caught = false;
    var f4 = SIMD.Float32x4(1,2,3,4);
    var i4 = SIMD.Int32x4(1,2,3,4);
    try {
        for (; i < 200; i++) {
            if (i % 2 == 0) {
                add(i, i4, i4);
            } else if (i == 199) {
                add(i, i4, f4);
            } else {
                add(i, f4, f4);
            }
        }
    } catch(e) {
        print(e);
        assertEq(e instanceof TypeError, true);
        assertEq(i, 199);
        caught = true;
    }

    assertEq(i < 199 || caught, true);

})();

