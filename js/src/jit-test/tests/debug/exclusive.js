var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
dbg.onEnterFrame = function () { };
var success = false;
try {
    dbg.collectCoverageInfo = true;
    success = true;
} catch (e) {
    assertEq(/onEnterFrame and collectCoverageInfo cannot be active at the same time/.test(e.message), true);
}
assertEq(success, false);

dbg.onEnterFrame = undefined;
dbg.collectCoverageInfo = true;

try {
    dbg.onEnterFrame = function () { };
    success = true;
} catch (e) {
    assertEq(/onEnterFrame and collectCoverageInfo cannot be active at the same time/.test(e.message), true);
}
assertEq(success, false);
dbg.collectCoverageInfo = false;
dbg.onEnterFrame = function () { };
