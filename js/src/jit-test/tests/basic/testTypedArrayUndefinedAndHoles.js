function holeArray(sparse) {
    var a = [,,];
    if (sparse)
        a.length = 1000;
    return a;
}

function undefinedArray(sparse) {
    var a = [ undefined, undefined, undefined ];
    if (sparse)
        a.length = 1000;
    return a;
}

var a;
a = new Int32Array(holeArray(false));
assertEq(a[0], 0);
a = new Int32Array(holeArray(true));
assertEq(a[0], 0);
a = new Int32Array(undefinedArray(false));
assertEq(a[0], 0);
a = new Int32Array(undefinedArray(true));
assertEq(a[0], 0);

a = new Float64Array(holeArray(false));
assertEq(a[0], NaN);
a = new Float64Array(holeArray(true));
assertEq(a[0], NaN);
a = new Float64Array(undefinedArray(false));
assertEq(a[0], NaN);
a = new Float64Array(undefinedArray(true));
assertEq(a[0], NaN);
