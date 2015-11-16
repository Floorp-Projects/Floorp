assertNear(Math.log10(2), 0.3010299956639812);
assertNear(Math.log10(7), 0.8450980400142568);
assertNear(Math.log10(Math.E), Math.LOG10E);

// FIXME
// On Mac OS X 10.7 32bit build, Math.log10(0.01) returns bfffffff fffffffe.
// See bug 1225024.
var sloppy_tolerance = 2;

for (var i = -10; i < 10; i++)
    assertNear(Math.log10(Math.pow(10, i)), i, sloppy_tolerance);

reportCompare(0, 0, 'ok');

