setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);
var i;

// Check that we are able to remove the operation inside recover test functions (denoted by "rop..."),
// when we inline the first version of uceFault, and ensure that the bailout is correct
// when uceFault is replaced (which cause an invalidation bailout)

var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
}

var uceFault_bitnot_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitnot_number'));
function rbitnot_number(i) {
    var x = ~i;
    if (uceFault_bitnot_number(i) || uceFault_bitnot_number(i))
        assertEq(x, -100  /* = ~99 */);
    return i;
}

var uceFault_bitnot_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitnot_object'));
function rbitnot_object(i) {
    var t = i;
    var o = { valueOf: function () { return t; } };
    var x = ~o; /* computed with t == i, not 1000 */
    t = 1000;
    if (uceFault_bitnot_object(i) || uceFault_bitnot_object(i))
        assertEq(x, -100  /* = ~99 */);
    return i;
}

var uceFault_bitor_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitor_number'));
function rbitor_number(i) {
    var x = i | -100; /* -100 == ~99 */
    if (uceFault_bitor_number(i) || uceFault_bitor_number(i))
        assertEq(x, -1) /* ~99 | 99 = -1 */
    return i;
}

var uceFault_bitor_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitor_object'));
function rbitor_object(i) {
    var t = i;
    var o = { valueOf: function() { return t; } };
    var x = o | -100;
    t = 1000;
    if (uceFault_bitor_object(i) || uceFault_bitor_object(i))
        assertEq(x, -1);
    return i;
}

var uceFault_bitxor_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitxor_number'));
function rbitxor_number(i) {
    var x = 1 ^ i;
    if (uceFault_bitxor_number(i) || uceFault_bitxor_number(i))
        assertEq(x, 98  /* = 1 XOR 99 */);
    return i;
}

var uceFault_bitxor_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_bitxor_object'));
function rbitxor_object(i) {
    var t = i;
    var o = { valueOf: function () { return t; } };
    var x = 1 ^ o; /* computed with t == i, not 1000 */
    t = 1000;
    if (uceFault_bitxor_object(i) || uceFault_bitxor_object(i))
        assertEq(x, 98  /* = 1 XOR 99 */);
    return i;
}

var uceFault_lsh_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_lsh_number'));
function rlsh_number(i) {
    var x = i << 1;
    if (uceFault_lsh_number(i) || uceFault_lsh_number(i))
        assertEq(x, 198); /* 99 << 1 == 198 */
    return i;
}

var uceFault_lsh_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_lsh_object'));
function rlsh_object(i) {
    var t = i;
    var o = { valueOf: function() { return t; } };
    var x = o << 1;
    t = 1000;
    if (uceFault_lsh_object(i) || uceFault_lsh_object(i))
        assertEq(x, 198);
    return i;
}

var uceFault_ursh_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_ursh_number'));
function rursh_number(i) {
    var x = i >>> 1;
    if (uceFault_ursh_number(i) || uceFault_ursh_number(i))
        assertEq(x, 49  /* = 99 >>> 1 */);
    return i;
}

var uceFault_ursh_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_ursh_object'));
function rursh_object(i) {
    var t = i;
    var o = { valueOf: function () { return t; } };
    var x = o >>> 1; /* computed with t == i, not 1000 */
    t = 1000;
    if (uceFault_ursh_object(i) || uceFault_ursh_object(i))
        assertEq(x, 49  /* = 99 >>> 1 */);
    return i;
}

var uceFault_add_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_add_number'));
function radd_number(i) {
    var x = 1 + i;
    if (uceFault_add_number(i) || uceFault_add_number(i))
        assertEq(x, 100  /* = 1 + 99 */);
    return i;
}

var uceFault_add_float = eval(uneval(uceFault).replace('uceFault', 'uceFault_add_float'));
function radd_float(i) {
    var t = Math.fround(1/3);
    var fi = Math.fround(i);
    var x = Math.fround(Math.fround(Math.fround(Math.fround(t + fi) + t) + fi) + t);
    if (uceFault_add_float(i) || uceFault_add_float(i))
        assertEq(x, 199); /* != 199.00000002980232 (when computed with double additions) */
    return i;
}

var uceFault_add_string = eval(uneval(uceFault).replace('uceFault', 'uceFault_add_string'));
function radd_string(i) {
    var x = "s" + i;
    if (uceFault_add_string(i) || uceFault_add_string(i))
        assertEq(x, "s99");
    return i;
}

var uceFault_add_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_add_object'));
function radd_object(i) {
    var t = i;
    var o = { valueOf: function () { return t; } };
    var x = o + i; /* computed with t == i, not 1000 */
    t = 1000;
    if (uceFault_add_object(i) || uceFault_add_object(i))
        assertEq(x, 198);
    return i;
}

for (i = 0; i < 100; i++) {
    rbitnot_number(i);
    rbitnot_object(i);
    rbitor_number(i);
    rbitor_object(i);
    rbitxor_number(i);
    rbitxor_object(i);
    rlsh_number(i);
    rlsh_object(i);
    rursh_number(i);
    rursh_object(i);
    radd_number(i);
    radd_float(i);
    radd_string(i);
    radd_object(i);
}

// Test that we can refer multiple time to the same recover instruction, as well
// as chaining recover instructions.

function alignedAlloc($size, $alignment) {
    var $1 = $size + 4 | 0;
    var $2 = $alignment - 1 | 0;
    var $3 = $1 + $2 | 0;
    var $4 = malloc($3);
}

function malloc($bytes) {
    var $189 = undefined;
    var $198 = $189 + 8 | 0;
}

for (i = 0; i < 50; i++)
    alignedAlloc(608, 16);
