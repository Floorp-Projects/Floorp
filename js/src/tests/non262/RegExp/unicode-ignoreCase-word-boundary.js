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

// Bug 1338779 - More testcases.
assertEq(/(i\B\u017F)/ui.test("is"), true);
assertEq(/(i\B\u017F)/ui.test("it"), false);
assertEq(/(i\B\u017F)+/ui.test("is"), true);
assertEq(/(i\B\u017F)+/ui.test("it"), false);

assertEq(/(\u017F\Bi)/ui.test("si"), true);
assertEq(/(\u017F\Bi)/ui.test("ti"), false);
assertEq(/(\u017F\Bi)+/ui.test("si"), true);
assertEq(/(\u017F\Bi)+/ui.test("ti"), false);

assertEq(/(i\B\u212A)/ui.test("ik"), true);
assertEq(/(i\B\u212A)/ui.test("it"), false);
assertEq(/(i\B\u212A)+/ui.test("ik"), true);
assertEq(/(i\B\u212A)+/ui.test("it"), false);

assertEq(/(\u212A\Bi)/ui.test("ki"), true);
assertEq(/(\u212A\Bi)/ui.test("ti"), false);
assertEq(/(\u212A\Bi)+/ui.test("ki"), true);
assertEq(/(\u212A\Bi)+/ui.test("ti"), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
