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

// This function is used to force a bailout when it is inlined, and to recover
// the frame of the function in which this function is inlined.
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
    var a = [1];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array1LengthBail(i) {
    var a = [1];
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2Length(i) {
    var a = [1, 2];
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2LengthBail(i) {
    var a = [1, 2];
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}


// Check Array content
function array1Content(i) {
    var a = [42];
    assertEq(a[0], 42);
    assertRecoveredOnBailout(a, true);
    return a.length;
}
function array1ContentBail0(i) {
    var a = [42];
    resumeHere(i);
    assertEq(a[0], 42);
    assertRecoveredOnBailout(a, true);
    return a.length;
}
function array1ContentBail1(i) {
    var a = [42];
    assertEq(a[0], 42);
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2Content(i) {
    var a = [1, 2];
    assertEq(a[0], 1);
    assertEq(a[1], 2);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail0(i) {
    var a = [1, 2];
    resumeHere(i);
    assertEq(a[0], 1);
    assertEq(a[1], 2);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail1(i) {
    var a = [1, 2];
    assertEq(a[0], 1);
    resumeHere(i);
    assertEq(a[1], 2);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function array2ContentBail2(i) {
    var a = [1, 2];
    assertEq(a[0], 1);
    assertEq(a[1], 2);
    resumeHere(i);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function arrayWrite1(i) {
    var a = [1, 2];
    a[0] = 42;
    assertEq(a[0], 42);
    assertEq(a[1], 2);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// We don't handle length sets yet.
function arrayWrite2(i) {
    var a = [1, 2];
    a.length = 1;
    assertEq(a[0], 1);
    assertEq(a[1], undefined);
    assertRecoveredOnBailout(a, false);
    return a.length;
}

function arrayWrite3(i) {
    var a = [1, 2, 0];
    if (i % 2 === 1)
	a[0] = 2;
    assertEq(a[0], 1 + (i % 2));
    assertRecoveredOnBailout(a, true);
    if (i % 2 === 1)
	bailout();
    assertEq(a[0], 1 + (i % 2));
    return a.length;
}

function arrayWrite4(i) {
    var a = [1, 2, 0];
    for (var x = 0; x < 2; x++) {
	if (x % 2 === 1)
	    bailout();
	else
	    a[0] = a[0] + 1;
    }
    assertEq(a[0], 2);
    assertEq(a[1], 2);
    assertRecoveredOnBailout(a, true);
    return a.length;
}

function arrayWriteDoubles(i) {
    var a = [0, 0, 0];
    a[0] = 3.14;
    // MConvertElementsToDoubles is only used for loads inside a loop.
    for (var x = 0; x < 2; x++) {
        assertEq(a[0], 3.14);
        assertEq(a[1], 0);
    }
    assertRecoveredOnBailout(a, true);
    return a.length;
}

// Check escape analysis in case of holes.
function arrayHole0(i) {
    var a = [1,,3];
    assertEq(a[0], 1);
    assertEq(a[1], undefined);
    assertEq(a[2], 3);
    // need to check for holes.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

// Same test as the previous one, but the Array.prototype is changed to return
// "100" when we request for the element "1".
function arrayHole1(i) {
    var a = [1,,3];
    assertEq(a[0], 1);
    assertEq(a[1], 100);
    assertEq(a[2], 3);
    // need to check for holes.
    assertRecoveredOnBailout(a, false);
    return a.length;
}

function build(l) { var arr = []; for (var i = 0; i < l; i++) arr.push(i); return arr }
var uceFault_arrayAlloc3 = eval(uneval(uceFault).replace('uceFault', 'uceFault_arrayAlloc3'));
function arrayAlloc(i) {
    var a = [0,1,2,3];
    if (uceFault_arrayAlloc3(i) || uceFault_arrayAlloc3(i)) {
        assertEq(a[0], 0);
        assertEq(a[3], 3);
        return a.length;
    }
    assertRecoveredOnBailout(a, true);
    return 0;
};

// Prevent compilation of the top-level
eval(uneval(resumeHere));

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
    arrayWrite1(i);
    arrayWrite2(i);
    arrayWrite3(i);
    arrayWrite4(i);
    arrayWriteDoubles(i);
    arrayHole0(i);
    arrayAlloc(i);
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
