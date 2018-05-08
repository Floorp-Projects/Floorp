// Test Math.floor() for IonMonkey.
// Requires --ion-eager to enter at the top of each loop.

var floorDTests = [
	[-0, -0],
	[0.49999999999999997, 0],
	[0.5, 0],
	[1.0, 1],
	[1.5, 1],
	[792.8, 792],
	[-0.1, -1],
	[-1.0001, -2],
	[-3.14, -4],
	[2137483649.5, 2137483649],
	[2137483648.5, 2137483648],
	[2137483647.1, 2137483647],
	[900000000000, 900000000000],
	[-0, -0],
	[-Infinity, -Infinity],
	[Infinity, Infinity],
	[NaN, NaN],
	[-2147483648.8, -2147483649],
	[-2147483649.8, -2147483650]
];

var floorITests = [
	[0, 0],
	[4, 4],
	[-1, -1],
	[-7, -7],
	[2147483648, 2147483648],
	[-2147483649, -2147483649]
];

// Typed functions to be compiled by Ion.
function floorD(x) { return Math.floor(x); }
function floorI(x) { return Math.floor(x); }

function test() {
	// Always run this function in the interpreter.
	with ({}) {}

	for (var i = 0; i < floorDTests.length; i++)
		assertEq(floorD(floorDTests[i][0]), floorDTests[i][1]);
	for (var i = 0; i < floorITests.length; i++)
		assertEq(floorI(floorITests[i][0]), floorITests[i][1]);
}

for (var i = 0; i < 40; i++)
	test();
