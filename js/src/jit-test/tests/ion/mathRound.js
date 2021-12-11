// Test Math.round() for IonMonkey.
// Requires --ion-eager to enter at the top of each loop.

var roundDTests = [
	[-0, -0],
	[0.49999999999999997, 0],
	[0.5, 1],
	[1.0, 1],
	[1.5, 2],
	[792.8, 793],
	[-0.1, -0],
	[-1.0001, -1],
	[-3.14, -3],
	[2137483649.5, 2137483650],
	[2137483648.5, 2137483649],
	[2137483647.1, 2137483647],
	[900000000000, 900000000000],
	[-0, -0],
	[-Infinity, -Infinity],
	[Infinity, Infinity],
	[NaN, NaN],
	[-2147483648.8, -2147483649],
	[-2147483649.8, -2147483650]
];

var roundITests = [
	[0, 0],
	[4, 4],
	[2147483648, 2147483648],
	[-2147483649, -2147483649]
];

// Typed functions to be compiled by Ion.
function roundD(x) { return Math.round(x); }
function roundI(x) { return Math.round(x); }

function test() {
	// Always run this function in the interpreter.
	with ({}) {}

	for (var i = 0; i < roundDTests.length; i++)
		assertEq(roundD(roundDTests[i][0]), roundDTests[i][1]);
	for (var i = 0; i < roundITests.length; i++)
		assertEq(roundI(roundITests[i][0]), roundITests[i][1]);
}

for (var i = 0; i < 40; i++)
	test();
