// Ion eager fails the test below because we have not yet created any
// template object in baseline before running the content of the top-level
// function.
if (getJitCompilerOptions()["ion.warmup.trigger"] <= 100)
    setJitCompilerOption("ion.warmup.trigger", 100);

// This test checks that we are able to remove the getelem & setelem with scalar
// replacement, so we should not force inline caches, as this would skip the
// generation of getelem & setelem instructions.
if (getJitCompilerOptions()["ion.forceinlineCaches"])
    setJitCompilerOption("ion.forceinlineCaches", 0);

// Prevent the GC from cancelling Ion compilations, when we expect them to succeed
gczeal(0);

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
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array0LengthBail(i) {
    var a = [];
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array1Length(i) {
    var a = [ i ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array1LengthBail(i) {
    var a = [ i ];
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2Length(i) {
    var a = [ i, i ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2LengthBail(i) {
    var a = [ i, i ];
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// Check that we can correctly gc in the middle of an incomplete object
// intialization.
function arrayWithGCInit0(i) {
    var a = [ (i == 99 ? (gc(), i) : i), i ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function arrayWithGCInit1(i) {
    var a = [ i, (i == 99 ? (gc(), i) : i) ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function arrayWithGCInit2(i) {
    var a = [ i, i ];
    if (i == 99) gc();
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// Check Array content
function array1Content(i) {
    var a = [ i ];
    assertEq(a[0], i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}
function array1ContentBail0(i) {
    var a = [ i ];
    resumeHere(i);
    assertEq(a[0], i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}
function array1ContentBail1(i) {
    var a = [ i ];
    assertEq(a[0], i);
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2Content(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    assertEq(a[1], i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail0(i) {
    var a = [ i, i ];
    resumeHere(i);
    assertEq(a[0], i);
    assertEq(a[1], i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail1(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    resumeHere(i);
    assertEq(a[1], i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail2(i) {
    var a = [ i, i ];
    assertEq(a[0], i);
    assertEq(a[1], i);
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// Check bailouts during the initialization.
function arrayInitBail0(i) {
    var a = [ resumeHere(i), i ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function arrayInitBail1(i) {
    var a = [ i, resumeHere(i) ];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// Check recovery of large arrays.
function arrayLarge0(i) {
    var a = new Array(10000000);
    resumeHere(); bailout(); // always resume here.
    // IsArrayEscaped prevent us from escaping Arrays with too many elements.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

function arrayLarge1(i) {
    var a = new Array(10000000);
    a[0] = i;
    assertEq(a[0], i);
    // IsArrayEscaped prevent us from escaping Arrays with too many elements.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

function arrayLarge2(i) {
    var a = new Array(10000000);
    a[0] = i;
    a[100] = i;
    assertEq(a[0], i);
    assertEq(a[100], i);
    // IsArrayEscaped prevent us from escaping Arrays with too many elements.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

// Check escape analysis in case of branches.
function arrayCond(i) {
    var a = [i,0,i];
    if (i % 2 == 1)
        a[1] = i;
    assertEq(a[0], i);
    assertEq(a[1], (i % 2) * i);
    assertEq(a[2], i);
    assertRecoveredOnBailout(a, true);
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
    // need to check for holes.
    assertRecoveredOnBailout(a, false);
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
    // need to check for holes.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

// Check that we correctly allocate the array after taking the recover path.
var uceFault_arrayAlloc0 = eval(`(${uceFault})`.replace('uceFault', 'uceFault_arrayAlloc0'));
function arrayAlloc0(i) {
    var a = new Array(10);
    if (uceFault_arrayAlloc0(i) || uceFault_arrayAlloc0(i)) {
        return a.length;
    }
    assertRecoveredOnBailout(a, true);
    return 0;
}

var uceFault_arrayAlloc1 = eval(`(${uceFault})`.replace('uceFault', 'uceFault_arrayAlloc1'));
function arrayAlloc1(i) {
    var a = new Array(10);
    if (uceFault_arrayAlloc1(i) || uceFault_arrayAlloc1(i)) {
        a[0] = i;
        a[1] = i;
        assertEq(a[0], i);
        assertEq(a[1], i);
        assertEq(a[2], undefined);
        return a.length;
    }
    assertRecoveredOnBailout(a, true);
    return 0;
}

var uceFault_arrayAlloc2 = eval(`(${uceFault})`.replace('uceFault', 'uceFault_arrayAlloc2'));
function arrayAlloc2(i) {
    var a = new Array(10);
    if (uceFault_arrayAlloc2(i) || uceFault_arrayAlloc2(i)) {
        a[4096] = i;
        assertEq(a[0], undefined);
        assertEq(a[4096], i);
        return a.length;
    }
    assertRecoveredOnBailout(a, true);
    return 0;
}

function build(l) { var arr = []; for (var i = 0; i < l; i++) arr.push(i); return arr }
var uceFault_arrayAlloc3 = eval(`(${uceFault})`.replace('uceFault', 'uceFault_arrayAlloc3'));
function arrayAlloc3(i) {
    var a = [0,1,2,3,4,5,6,7,8];
    if (uceFault_arrayAlloc3(i) || uceFault_arrayAlloc3(i)) {
        assertEq(a[0], 0);
        assertEq(a[3], 3);
        return a.length;
    }
    assertRecoveredOnBailout(a, true);
    return 0;
};

// Prevent compilation of the top-level
eval(`(${resumeHere})`);

for (var i = 0; i < 100; i++) {
    array0Length(i);
    array0LengthBail(i);
    array1Length(i);
    array1LengthBail(i);
    array2Length(i);
    array2LengthBail(i);
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
    //arrayCond(i); See bug 1697691.
    arrayHole0(i);
    arrayAlloc0(i);
    arrayAlloc1(i);
    arrayAlloc2(i);
    arrayAlloc3(i);
}

for (var i = 0; i < 100; i++) {
    arrayWithGCInit0(i);
    arrayWithGCInit1(i);
    arrayWithGCInit2(i);
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
