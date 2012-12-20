function intLength (a, l) {
    var res = 0;
    for (var i = 0; i < l; i++)
        res += a.length;
    return res / l;
}

function valueLength (a, l) {
    var res = 0;
    for (var i = 0; i < l; i++)
        res += a.length;
    return res / l;
}

var denseArray = [0,1,2,3,4,5,6,7,8,9];
var typedArray = new Uint8Array(10);
var hugeArray  = new Array(4294967295);
var fakeArray1 = { length: 10 };
var fakeArray2 = { length: 10.5 };

// Check the interpreter result and play with TI type objects.
assertEq(intLength(denseArray, 10), 10);
assertEq(intLength(typedArray, 10), 10);
// assertEq(intLength(fakeArray1, 10), 10);

assertEq(valueLength(denseArray, 10), 10);
assertEq(valueLength(typedArray, 10), 10);
assertEq(valueLength(hugeArray , 10), 4294967295);
assertEq(valueLength(fakeArray2, 10), 10.5);

// Heat up to compile (either JM / Ion)
assertEq(intLength(denseArray, 100), 10);
assertEq(valueLength(denseArray, 100), 10);

// No bailout should occur during any of the following checks:

// Check get-property length IC with dense array.
assertEq(intLength(denseArray, 1), 10);
assertEq(valueLength(denseArray, 1), 10);

// Check get-property length IC with typed array.
assertEq(intLength(typedArray, 1), 10);
assertEq(valueLength(typedArray, 1), 10);

// Check length which do not fit on non-double value.
assertEq(valueLength(hugeArray, 1), 4294967295);

// Check object length property.
assertEq(intLength(fakeArray1, 1), 10);
assertEq(valueLength(fakeArray2, 1), 10.5);

// Cause invalidation of intLength by returning a double.
assertEq(intLength(hugeArray, 1), 4294967295);
assertEq(intLength(fakeArray2, 1), 10.5);
