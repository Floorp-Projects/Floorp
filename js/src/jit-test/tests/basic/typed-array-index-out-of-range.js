
// Out of bounds writes on typed arrays are uneffectful for all integers.

var x = new Int32Array(10);

function f() {
    for (var i = -100; i < 100; i++) {
	x[i] = i + 1;
	if (i >= 0 && i < 10)
	    assertEq(x[i], i + 1);
	else
	    assertEq(x[i], undefined);
    }
}
f();

// Integers which don't fit in a double value's mantissa aren't really integers.

var bigint = "" + Math.pow(2, 53);
x[bigint] = "twelve";
assertEq(x[bigint], "twelve");

// Infinity and -Infinity maybe are supposed to be integers, but they aren't currently.

x["Infinity"] = "twelve";
assertEq(x["Infinity"], "twelve");

x["-Infinity"] = "twelve";
assertEq(x["-Infinity"], "twelve");
