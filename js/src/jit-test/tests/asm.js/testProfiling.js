load(libdir + "asm.js");
load(libdir + "asserts.js");

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit();

function checkSubSequence(got, expect)
{
    var got_i = 0;
    EXP: for (var exp_i = 0; exp_i < expect.length; exp_i++) {
        var item = expect[exp_i];
        // Scan for next match in got.
        while (got_i < got.length) {
            if (got[got_i++] == expect[exp_i])
                continue EXP;
        }
        print("MISMATCH: " + got.join(",") + "\n" +
              "    VS    " + expect.join(","));
        return false;
    }
    return true;
}

function assertStackContainsSeq(got, expect)
{
    var normalized = [];

    for (var i = 0; i < got.length; i++) {
        if (got[i].length == 0)
            continue;
        var parts = got[i].split(',');
        for (var j = 0; j < parts.length; j++) {
            var frame = parts[j];
            frame = frame.replace(/ \([^\)]*\)/g, "");
            frame = frame.replace(/(fast|slow) FFI trampoline/g, "<");
            frame = frame.replace(/entry trampoline/g, ">");
            frame = frame.replace(/(\/[^\/,<]+)*\/testProfiling.js/g, "");
            frame = frame.replace(/testBuiltinD2D/g, "");
            frame = frame.replace(/testBuiltinF2F/g, "");
            frame = frame.replace(/testBuiltinDD2D/g, "");
            frame = frame.replace(/assertThrowsInstanceOf/g, "");
            frame = frame.replace(/^ffi[12]?/g, "");
            normalized.push(frame);
        }
    }

    var gotNorm = normalized.join(',').replace(/,+/g, ",");
    gotNorm = gotNorm.replace(/^,/, "").replace(/,$/, "");

    assertEq(checkSubSequence(gotNorm.split(','), expect.split(',')), true);
}

// Test profiling enablement while asm.js is running.
var stacks;
var ffi = function(enable) {
    if (enable == +1)
        enableSPSProfiling();
    enableSingleStepProfiling();
    stacks = disableSingleStepProfiling();
    if (enable == -1)
        disableSPSProfiling();
}
var f = asmLink(asmCompile('global','ffis',USE_ASM + "var ffi=ffis.ffi; function g(i) { i=i|0; ffi(i|0) } function f(i) { i=i|0; g(i|0) } return f"), null, {ffi});
f(0);
assertStackContainsSeq(stacks, "");
f(+1);
assertStackContainsSeq(stacks, "");
f(0);
assertStackContainsSeq(stacks, "<,g,f,>");
f(-1);
assertStackContainsSeq(stacks, "<,g,f,>");
f(0);
assertStackContainsSeq(stacks, "");

// Enable profiling for the rest of the tests.
enableSPSProfiling();

var f = asmLink(asmCompile(USE_ASM + "function f() { return 42 } return f"));
enableSingleStepProfiling();
assertEq(f(), 42);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f,>,>");

var m = asmCompile(USE_ASM + "function g(i) { i=i|0; return (i+1)|0 } function f() { return g(42)|0 } return f");
for (var i = 0; i < 3; i++) {
    var f = asmLink(m);
    enableSingleStepProfiling();
    assertEq(f(), 43);
    var stacks = disableSingleStepProfiling();
    assertStackContainsSeq(stacks, ">,f,>,g,f,>,f,>,>");
}

var m = asmCompile(USE_ASM + "function g1() { return 1 } function g2() { return 2 } function f(i) { i=i|0; return TBL[i&1]()|0 } var TBL=[g1,g2]; return f");
for (var i = 0; i < 3; i++) {
    var f = asmLink(m);
    enableSingleStepProfiling();
    assertEq(f(0), 1);
    assertEq(f(1), 2);
    var stacks = disableSingleStepProfiling();
    assertStackContainsSeq(stacks, ">,f,>,g1,f,>,f,>,>,>,f,>,g2,f,>,f,>,>");
}

function testBuiltinD2D(name) {
    var m = asmCompile('g', USE_ASM + "var fun=g.Math." + name + "; function f(d) { d=+d; return +fun(d) } return f");
    for (var i = 0; i < 3; i++) {
        var f = asmLink(m, this);
        enableSingleStepProfiling();
        assertEq(f(.1), eval("Math." + name + "(.1)"));
        var stacks = disableSingleStepProfiling();
        assertStackContainsSeq(stacks, ">,f,>,native call,>,f,>,>");
    }
}
for (name of ['sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'ceil', 'floor', 'exp', 'log'])
    testBuiltinD2D(name);

function testBuiltinF2F(name) {
    var m = asmCompile('g', USE_ASM + "var tof=g.Math.fround; var fun=g.Math." + name + "; function f(d) { d=tof(d); return tof(fun(d)) } return f");
    for (var i = 0; i < 3; i++) {
        var f = asmLink(m, this);
        enableSingleStepProfiling();
        assertEq(f(.1), eval("Math.fround(Math." + name + "(Math.fround(.1)))"));
        var stacks = disableSingleStepProfiling();
        assertStackContainsSeq(stacks, ">,f,>,native call,>,f,>,>");
    }
}
for (name of ['ceil', 'floor'])
    testBuiltinF2F(name);

function testBuiltinDD2D(name) {
    var m = asmCompile('g', USE_ASM + "var fun=g.Math." + name + "; function f(d, e) { d=+d; e=+e; return +fun(d,e) } return f");
    for (var i = 0; i < 3; i++) {
        var f = asmLink(m, this);
        enableSingleStepProfiling();
        assertEq(f(.1, .2), eval("Math." + name + "(.1, .2)"));
        var stacks = disableSingleStepProfiling();
        assertStackContainsSeq(stacks, ">,f,>,native call,>,f,>,>");
    }
}
for (name of ['atan2', 'pow'])
    testBuiltinDD2D(name);

// FFI tests:
setJitCompilerOption("ion.warmup.trigger", 10);
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);

var m = asmCompile('g','ffis', USE_ASM + "var ffi1=ffis.ffi1, ffi2=ffis.ffi2; function f() { return ((ffi1()|0) + (ffi2()|0))|0 } return f");

var ffi1 = function() { return 10 }
var ffi2 = function() { return 73 }
var f = asmLink(m, null, {ffi1,ffi2});

// Interp FFI exit
enableSingleStepProfiling();
assertEq(f(), 83);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f,>,<,f,>,f,>,<,f,>,f,>,>");

// Ion FFI exit
for (var i = 0; i < 20; i++)
    assertEq(f(), 83);
enableSingleStepProfiling();
assertEq(f(), 83);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f,>,<,f,>,f,>,<,f,>,f,>,>");

var ffi1 = function() { return { valueOf() { return 20 } } }
var ffi2 = function() { return { valueOf() { return 74 } } }
var f = asmLink(m, null, {ffi1,ffi2});

// Interp FFI exit
enableSingleStepProfiling();
assertEq(f(), 94);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f,>,<,f,>,f,>,<,f,>,f,>,>"); // TODO: add 'valueOf' once interp shows up

// Ion FFI exit
for (var i = 0; i < 20; i++)
    assertEq(f(), 94);
enableSingleStepProfiling();
assertEq(f(), 94);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f,>,<,f,>,f,>,<,f,>,f,>,>"); // TODO: add 'valueOf' once interp shows up

var ffi1 = function() { return 15 }
var ffi2 = function() { return f2() + 17 }
var {f1,f2} = asmLink(asmCompile('g','ffis', USE_ASM + "var ffi1=ffis.ffi1, ffi2=ffis.ffi2; function f2() { return ffi1()|0 } function f1() { return ffi2()|0 } return {f1:f1, f2:f2}"), null, {ffi1, ffi2});
// Interpreter FFI exit
enableSingleStepProfiling();
assertEq(f1(), 32);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f1,>,<,f1,>,>,<,f1,>,f2,>,<,f1,>,<,f2,>,<,f1,>,f2,>,<,f1,>,>,<,f1,>,<,f1,>,f1,>,>");


// Ion FFI exit
for (var i = 0; i < 20; i++)
    assertEq(f1(), 32);
enableSingleStepProfiling();
assertEq(f1(), 32);
var stacks = disableSingleStepProfiling();
assertStackContainsSeq(stacks, ">,f1,>,<,f1,>,>,<,f1,>,f2,>,<,f1,>,<,f2,>,<,f1,>,f2,>,<,f1,>,>,<,f1,>,<,f1,>,f1,>,>");


if (isSimdAvailable() && typeof SIMD !== 'undefined') {
    // SIMD out-of-bounds exit
    var buf = new ArrayBuffer(0x10000);
    var f = asmLink(asmCompile('g','ffi','buf', USE_ASM + 'var f4=g.SIMD.float32x4; var f4l=f4.load; var u8=new g.Uint8Array(buf); function f(i) { i=i|0; return f4l(u8, 0xFFFF + i | 0); } return f'), this, {}, buf);
    enableSingleStepProfiling();
    assertThrowsInstanceOf(() => f(4), RangeError);
    var stacks = disableSingleStepProfiling();
    // TODO check that expected is actually the correctly expected string, when
    // SIMD is implemented on ARM.
    assertStackContainsSeq(stacks, ">,f,>,inline stub,f,>");
}


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
