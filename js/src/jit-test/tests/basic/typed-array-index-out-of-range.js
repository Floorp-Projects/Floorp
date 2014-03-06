
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

// Really big integers not representable with a double or uint64 are still integers.

var bigint = "" + Math.pow(2, 53);
x[bigint] = "twelve";
assertEq(x[bigint], undefined);

x["9999999999999999999999"] = "twelve";
assertEq(x["9999999999999999999999"], undefined);

// Except when their toString() makes them not look like integers!
x[9999999999999999999999] = "twelve";
assertEq(x[9999999999999999999999], "twelve");

// Infinity and -Infinity maybe are supposed to be integers, but they aren't currently.

x["Infinity"] = "twelve";
assertEq(x["Infinity"], "twelve");

x["-Infinity"] = "twelve";
assertEq(x["-Infinity"], "twelve");
