load(libdir + "asm.js");

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit();

function assertEqualStacks(got, expect)
{
    // Strip off the " (script/library info)"
    got = String(got).replace(/ \([^\)]*\)/g, "");

    // Shorten FFI/entry trampolines
    got = got.replace(/FFI trampoline/g, "<").replace(/entry trampoline/g, ">");

    assertEq(got, expect);
}

// Test profiling enablement while asm.js is running.
var stacks;
var ffi = function(enable) {
    if (enable == +1)
        enableSPSProfiling();
    if (enable == -1)
        disableSPSProfiling();
    enableSingleStepProfiling();
    stacks = disableSingleStepProfiling();
}
var f = asmLink(asmCompile('global','ffis',USE_ASM + "var ffi=ffis.ffi; function g(i) { i=i|0; ffi(i|0) } function f(i) { i=i|0; g(i|0) } return f"), null, {ffi});
f(0);
assertEqualStacks(stacks, "");
f(+1);
assertEqualStacks(stacks, "");
f(0);
assertEqualStacks(stacks, "<gf>");
f(-1);
assertEqualStacks(stacks, "<gf>");
f(0);
assertEqualStacks(stacks, "");

// Enable profiling for the rest of the tests.
enableSPSProfiling();

var f = asmLink(asmCompile(USE_ASM + "function f() { return 42 } return f"));
enableSingleStepProfiling();
assertEq(f(), 42);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f>,>,");

var f = asmLink(asmCompile(USE_ASM + "function g(i) { i=i|0; return (i+1)|0 } function f() { return g(42)|0 } return f"));
enableSingleStepProfiling();
assertEq(f(), 43);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f>,gf>,f>,>,");

var f = asmLink(asmCompile(USE_ASM + "function g1() { return 1 } function g2() { return 2 } function f(i) { i=i|0; return TBL[i&1]()|0 } var TBL=[g1,g2]; return f"));
enableSingleStepProfiling();
assertEq(f(0), 1);
assertEq(f(1), 2);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f>,g1f>,f>,>,,>,f>,g2f>,f>,>,");

function testBuiltinD2D(name) {
    var f = asmLink(asmCompile('g', USE_ASM + "var fun=g.Math." + name + "; function f(d) { d=+d; return +fun(d) } return f"), this);
    enableSingleStepProfiling();
    assertEq(f(.1), eval("Math." + name + "(.1)"));
    var stacks = disableSingleStepProfiling();
    assertEqualStacks(stacks, ",>,f>,Math." + name + "f>,f>,>,");
}
for (name of ['sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'ceil', 'floor', 'exp', 'log'])
    testBuiltinD2D(name);
function testBuiltinF2F(name) {
    var f = asmLink(asmCompile('g', USE_ASM + "var tof=g.Math.fround; var fun=g.Math." + name + "; function f(d) { d=tof(d); return tof(fun(d)) } return f"), this);
    enableSingleStepProfiling();
    assertEq(f(.1), eval("Math.fround(Math." + name + "(Math.fround(.1)))"));
    var stacks = disableSingleStepProfiling();
    assertEqualStacks(stacks, ",>,f>,Math." + name + "f>,f>,>,");
}
for (name of ['ceil', 'floor'])
    testBuiltinF2F(name);
function testBuiltinDD2D(name) {
    var f = asmLink(asmCompile('g', USE_ASM + "var fun=g.Math." + name + "; function f(d, e) { d=+d; e=+e; return +fun(d,e) } return f"), this);
    enableSingleStepProfiling();
    assertEq(f(.1, .2), eval("Math." + name + "(.1, .2)"));
    var stacks = disableSingleStepProfiling();
    assertEqualStacks(stacks, ",>,f>,Math." + name + "f>,f>,>,");
}
for (name of ['atan2', 'pow'])
    testBuiltinDD2D(name);

// FFI tests:
setJitCompilerOption("ion.usecount.trigger", 10);
setJitCompilerOption("baseline.usecount.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);

var ffi1 = function() { return 10 }
var ffi2 = function() { return 73 }
var f = asmLink(asmCompile('g','ffis', USE_ASM + "var ffi1=ffis.ffi1, ffi2=ffis.ffi2; function f() { return ((ffi1()|0) + (ffi2()|0))|0 } return f"), null, {ffi1,ffi2});
// Interpreter FFI exit
enableSingleStepProfiling();
assertEq(f(), 83);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f>,<f>,f>,<f>,f>,>,");
// Ion FFI exit
for (var i = 0; i < 20; i++)
    assertEq(f(), 83);
enableSingleStepProfiling();
assertEq(f(), 83);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f>,<f>,f>,<f>,f>,>,");

var ffi1 = function() { return 15 }
var ffi2 = function() { return f2() + 17 }
var {f1,f2} = asmLink(asmCompile('g','ffis', USE_ASM + "var ffi1=ffis.ffi1, ffi2=ffis.ffi2; function f2() { return ffi1()|0 } function f1() { return ffi2()|0 } return {f1:f1, f2:f2}"), null, {ffi1, ffi2});
// Interpreter FFI exit
enableSingleStepProfiling();
assertEq(f1(), 32);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f1>,<f1>,><f1>,f2><f1>,<f2><f1>,f2><f1>,><f1>,<f1>,f1>,>,");
// Ion FFI exit
for (var i = 0; i < 20; i++)
    assertEq(f1(), 32);
enableSingleStepProfiling();
assertEq(f1(), 32);
var stacks = disableSingleStepProfiling();
assertEqualStacks(stacks, ",>,f1>,<f1>,><f1>,f2><f1>,<f2><f1>,f2><f1>,><f1>,<f1>,f1>,>,");

// This takes forever to run.
// Stack-overflow exit test
//var limit = -1;
//var maxct = 0;
//function ffi(ct) { if (ct == limit) { enableSingleStepProfiling(); print("enabled"); } maxct = ct; }
//var f = asmLink(asmCompile('g', 'ffis',USE_ASM + "var ffi=ffis.ffi; var ct=0; function rec(){ ct=(ct+1)|0; ffi(ct|0); rec() } function f() { ct=0; rec() } return f"), null, {ffi});
//// First find the stack limit:
//var caught = false;
//try { f() } catch (e) { caught = true; assertEq(String(e).indexOf("too much recursion") >= 0, true) }
//assertEq(caught, true);
//limit = maxct;
//print("Setting limit");
//var caught = false;
//try { f() } catch (e) { caught = true; print("caught"); assertEq(String(e).indexOf("too much recursion") >= 0, true) }
//var stacks = disableSingleStepProfiling();
//assertEq(String(stacks).indexOf("rec") >= 0, true);
