var BUGNUMBER = 1338373;
var summary = "Word boundary should match U+017F and U+212A in unicode+ignoreCase.";

assertEq(/\b/iu.test('\u017F'), true);
assertEq(/\b/i.test('\u017F'), false);
assertEq(/\b/u.test('\u017F'), false);
assertEq(/\b/.test('\u017F'), false);

assertEq(/\b/iu.test('\u212A'), true);
assertEq(/\b/i.test('\u212A'), false);
assertEq(/\b/u.test('\u212A'), false);
assertEq(/\b/.test('\u212A'), false);

assertEq(/\B/iu.test('\u017F'), false);
assertEq(/\B/i.test('\u017F'), true);
assertEq(/\B/u.test('\u017F'), true);
assertEq(/\B/.test('\u017F'), true);

assertEq(/\B/iu.test('\u212A'), false);
assertEq(/\B/i.test('\u212A'), true);
assertEq(/\B/u.test('\u212A'), true);
assertEq(/\B/.test('\u212A'), true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
