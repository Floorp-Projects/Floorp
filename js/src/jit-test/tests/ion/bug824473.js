function dumpArgs(i) { if (i == 90) return funapply.arguments.length; return [i]; }
function funapply() { return dumpArgs.apply(undefined, arguments); }
function test(i) { return funapply(i); }

assertEq(test(89)[0], 89);
assertEq(test(90), 1);

function dumpArgs2(i,b) { if (i == 90) return funapply2.arguments.length; return [i]; }
function funapply2() { return dumpArgs2.apply(undefined, arguments); }
function test2(i,b) { return funapply2(i,b); }

assertEq(test2(89, 10)[0], 89);
assertEq(test2(90, 10), 2);

function dumpArgs3(i,b) { if (i == 90) return funapply3.arguments.length; return [i]; }
function funapply3() { return dumpArgs3.apply(undefined, arguments); }
function test3(i,b, c) { return funapply3(i,b,c); }

assertEq(test3(89, 10, 11)[0], 89);
assertEq(test3(90, 10, 11), 3);

function dumpArgs4(i) { if (i == 90) return funapply4.arguments.length; return [i]; }
function funapply4() { return dumpArgs4.apply(undefined, arguments); }
function test4(i,b) { return funapply4(i,b,1,2); }

assertEq(test4(89,10)[0], 89);
assertEq(test4(90,10), 4);

function dumpArgs5(i,j,k,l) { if (i == 90) return funapply5.arguments.length*10 + l; return [i]; }
function funapply5() { return dumpArgs5.apply(undefined, arguments); }
function test5(i,b) { return funapply5(i,b,1,2); }

assertEq(test5(89,10)[0], 89);
assertEq(test5(90,10), 42);

function dumpArgs6(i) { if (i == 90) return funapply6.arguments.length; return [i]; }
function funapply6() { return dumpArgs6.apply(undefined, arguments); }
function test6(i) { return funapply6(i,1,2,3); }

assertEq(test6(89)[0], 89);
assertEq(test6(90), 4);
