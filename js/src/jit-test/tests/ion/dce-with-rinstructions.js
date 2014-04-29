setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);
var i;

// Check that we are able to remove the addition inside "ra" functions, when we
// inline the first version of uceFault, and ensure that the bailout is correct
// when uceFault is replaced (which cause an invalidation bailout)

var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
}

var uceFault_number = eval(uneval(uceFault).replace('uceFault', 'uceFault_number'));
function ra_number(i) {
    var x = 1 + i;
    if (uceFault_number(i) || uceFault_number(i))
        assertEq(x, 100  /* = 1 + 99 */);
    return i;
}

var uceFault_float = eval(uneval(uceFault).replace('uceFault', 'uceFault_float'));
function ra_float(i) {
    var t = Math.fround(1/3);
    var fi = Math.fround(i);
    var x = Math.fround(Math.fround(Math.fround(Math.fround(t + fi) + t) + fi) + t);
    if (uceFault_float(i) || uceFault_float(i))
        assertEq(x, 199); /* != 199.00000002980232 (when computed with double additions) */
    return i;
}

var uceFault_string = eval(uneval(uceFault).replace('uceFault', 'uceFault_string'));
function ra_string(i) {
    var x = "s" + i;
    if (uceFault_string(i) || uceFault_string(i))
        assertEq(x, "s99");
    return i;
}

var uceFault_object = eval(uneval(uceFault).replace('uceFault', 'uceFault_object'));
function ra_object(i) {
    var x = {} + i;
    if (uceFault_object(i) || uceFault_object(i))
        assertEq(x, "[object Object]99");
    return i;
}

for (i = 0; i < 100; i++) {
    ra_number(i);
    ra_float(i);
    ra_string(i);
    ra_object(i);
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
