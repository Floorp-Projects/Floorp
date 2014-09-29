// Removing a global as a debuggee forgets all its breakpoints.


var dbgA = new Debugger;
var logA = '';

var dbgB = new Debugger;
var logB = '';

var g1 = newGlobal();
g1.eval('function g1f() { print("Weltuntergang"); }');
var DOAg1 = dbgA.addDebuggee(g1);
var DOAg1f = DOAg1.getOwnPropertyDescriptor('g1f').value;
DOAg1f.script.setBreakpoint(0, { hit: () => { logA += '1'; } });

var DOBg1 = dbgB.addDebuggee(g1);
var DOBg1f = DOBg1.getOwnPropertyDescriptor('g1f').value;
DOBg1f.script.setBreakpoint(0, { hit: () => { logB += '1'; } });


var g2 = newGlobal();
g2.eval('function g2f() { print("Mokushi"); }');

var DOAg2 = dbgA.addDebuggee(g2);
var DOAg2f = DOAg2.getOwnPropertyDescriptor('g2f').value;
DOAg2f.script.setBreakpoint(0, { hit: () => { logA += '2'; } });

var DOBg2 = dbgB.addDebuggee(g2);
var DOBg2f = DOBg2.getOwnPropertyDescriptor('g2f').value;
DOBg2f.script.setBreakpoint(0, { hit: () => { logB += '2'; } });

assertEq(logA, '');
assertEq(logB, '');
g1.g1f();
g2.g2f();
assertEq(logA, '12');
assertEq(logB, '12');
logA = logB = '';

// Removing a global as a debuggee should make its breakpoint not hit.
dbgA.removeDebuggee(g2);
dbgB.removeDebuggee(g1);
assertEq(logA, '');
assertEq(logB, '');
g1.g1f();
g2.g2f();
assertEq(logA, '1');
assertEq(logB, '2');
logA = logB = '';

// Adding the global back as a debuggee should not resurrect its breakpoints.
dbgA.addDebuggee(g2);
dbgB.addDebuggee(g1);
assertEq(logA, '');
assertEq(logB, '');
g1.g1f();
g2.g2f();
assertEq(logA, '1');
assertEq(logB, '2');
logA = logB = '';

// But, we can set them again, and it all works.
DOAg2f.script.setBreakpoint(0, { hit: () => { logA += '2'; } });
assertEq(logA, '');
assertEq(logB, '');
g2.g2f();
g1.g1f();
assertEq(logA, '21');
assertEq(logB, '2');
logA = logB = '';

DOBg1f.script.setBreakpoint(0, { hit: () => { logB += '1'; } });
assertEq(logA, '');
assertEq(logB, '');
g2.g2f();
g1.g1f();
assertEq(logA, '21');
assertEq(logB, '21');
