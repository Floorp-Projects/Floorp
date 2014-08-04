//setJitCompilerOption("baseline.usecount.trigger", 10);
//setJitCompilerOption("ion.usecount.trigger", 20);

// This function is used to force a bailout when it is inlined, and to recover
// the frame which is inlining this function.
var resumeHere = function (i) { if (i >= 99) bailout(); };

// This function is used to cause an invalidation after having removed a branch
// after DCE.  This is made to check if we correctly recover an array
// allocation.
var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
};

// This function is used to ensure that we do escape the array, and thus prevent
// any escape analysis.
var global_arr;
function escape(arr) { global_arr = arr; }

// Check Array length defined by the literal.
function array0Length(i) {
    var a = [];
    return a.length;
}

function array0LengthBail(i) {
    var a = [];
    resumeHere(i);
    return a.length;
}

function array1Length(i) {
    var a = [ i ];
    return a.length;
}

function array1LengthBail(i) {
    var a = [ i ];
    resumeHere(i);
    return a.length;
}

function array2Length(i) {
    var a = [ i, i ];
    return a.length;
}

function array2LengthBail(i) {
    var a = [ i, i ];
    resumeHere(i);
    return a.length;
}

// Check that we can correctly gc in the middle of an incomplete object
// intialization.
function arrayWithGCInit(i) {
    var a = [ gc(), gc() ];
    gc();
    return a.length;
}

// Check Array content
function array1Content(i) {
    var a = [ i ];
    assertEq(a[0], i);
    return a.length;
}
function array1ContentBail0(i) {
    var a = [ i ];
    resumeHere(i);
    assertEq(a[0], i);
    return a.length;
}
function array1ContentBail1(i) {
    var a = [ i ];
    assertEq(a[0], i);
    resumeHere(i);
    return a.length;
}

function array2Content(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    assertEq(a[1], i);
    return a.length;
}

function array2ContentBail0(i) {
    var a = [ i, i ];
    resumeHere(i);
    assertEq(a[0], i);
    assertEq(a[1], i);
    return a.length;
}

function array2ContentBail1(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    resumeHere(i);
    assertEq(a[1], i);
    return a.length;
}

function array2ContentBail2(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    assertEq(a[1], i);
    resumeHere(i);
    return a.length;
}

// Check bailouts during the initialization.
function arrayInitBail0(i) {
    var a = [ resumeHere(i), i ];
    return a.length;
}

function arrayInitBail1(i) {
    var a = [ i, resumeHere(i) ];
    return a.length;
}

// Check recovery of large arrays.
function arrayLarge0(i) {
    var a = new Array(10000000);
    resumeHere(); bailout(); // always resume here.
    return a.length;
}

function arrayLarge1(i) {
    var a = new Array(10000000);
    a[0] = i;
    assertEq(a[0], i);
    return a.length;
}

function arrayLarge2(i) {
    var a = new Array(10000000);
    a[0] = i;
    a[100] = i;
    assertEq(a[0], i);
    assertEq(a[100], i);
    return a.length;
}

// Check escape analysis in case of holes.
function arrayHole0(i) {
    var a = [i,,i];
    if (i != 99)
        a[1] = i;
    assertEq(a[0], i);
    assertEq(a[1], i != 99 ? i : undefined);
    assertEq(a[2], i);
    return a.length;
}

// Same test as the previous one, but the Array.prototype is changed to reutn
// "100" when we request for the element "1".
function arrayHole1(i) {
    var a = [i,,i];
    if (i != 99)
        a[1] = i;
    assertEq(a[0], i);
    assertEq(a[1], i != 99 ? i : 100);
    assertEq(a[2], i);
    return a.length;
}

// Check that we correctly allocate the array after taking the recover path.
var uceFault_arrayAlloc0 = eval(uneval(uceFault).replace('uceFault', 'uceFault_arrayAlloc0'));
function arrayAlloc0(i) {
    var a = new Array(10000000);
    if (uceFault_arrayAlloc0(i) || uceFault_arrayAlloc0(i)) {
        return a.length;
    }
    return 0;
}

var uceFault_arrayAlloc1 = eval(uneval(uceFault).replace('uceFault', 'uceFault_arrayAlloc1'));
function arrayAlloc1(i) {
    var a = new Array(10000000);
    if (uceFault_arrayAlloc0(i) || uceFault_arrayAlloc0(i)) {
        a[0] = i;
        a[1] = i;
        assertEq(a[0], i);
        assertEq(a[1], i);
        assertEq(a[2], undefined);
        return a.length;
    }
    return 0;
}

var uceFault_arrayAlloc2 = eval(uneval(uceFault).replace('uceFault', 'uceFault_arrayAlloc2'));
function arrayAlloc2(i) {
    var a = new Array(10000000);
    if (uceFault_arrayAlloc0(i) || uceFault_arrayAlloc0(i)) {
        a[4096] = i;
        assertEq(a[0], undefined);
        assertEq(a[4096], i);
        return a.length;
    }
    return 0;
}

function build(l) { var arr = []; for (var i = 0; i < l; i++) arr.push(i); return arr }
var uceFault_arrayAlloc3 = eval(uneval(uceFault).replace('uceFault', 'uceFault_arrayAlloc3'));
var arrayAlloc3 = function (i) {
    var a = [1,2];
    if (uceFault_arrayAlloc0(i) || uceFault_arrayAlloc0(i)) {
        assertEq(a[0], 0);
        assertEq(a[4095], 4095);
        return a.length;
    }
    return 0;
};
var arrayAlloc3 = eval(uneval(arrayAlloc3).replace('[1,2]', '[' + build(4096).join(",") + ']'));

// Prevent compilation of the top-level
eval(uneval(resumeHere));

for (var i = 0; i < 100; i++) {
    array0Length(i);
    array0LengthBail(i);
    array1Length(i);
    array1LengthBail(i);
    array2Length(i);
    array2LengthBail(i);
    arrayWithGCInit(i);
    array1Content(i);
    array1ContentBail0(i);
    array1ContentBail1(i);
    array2Content(i);
    array2ContentBail0(i);
    array2ContentBail1(i);
    array2ContentBail2(i);
    arrayInitBail0(i);
    arrayInitBail1(i);
    arrayLarge0(i);
    arrayLarge1(i);
    arrayLarge2(i);
    arrayHole0(i);
    arrayAlloc0(i);
    arrayAlloc1(i);
    arrayAlloc2(i);
    arrayAlloc3(i);
}

// If arr[1] is not defined, then we fallback on the prototype which instead of
// returning undefined, returns "0".
Object.defineProperty(Array.prototype, 1, {
  value: 100,
  configurable: true,
  enumerable: true,
  writable: true
});

for (var i = 0; i < 100; i++) {
    arrayHole1(i);
}
