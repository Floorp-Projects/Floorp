load(libdir + "asm.js");

setCachingEnabled(true);
if (!isAsmJSCompilationAvailable() || !isCachingEnabled())
    quit();

var body1 = "'use asm'; function f() { return 42 } function ff() { return 43 } return f";
var m = new Function(body1);
assertEq(isAsmJSModule(m), true);
assertEq(m()(), 42);
var m = new Function(body1);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m()(), 42);
var body2 = body1 + "f";
var m = new Function(body2);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
assertEq(m()(), 43);
var evalStr1 = "(function() { " + body1 + "})";
var m = eval(evalStr1);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
assertEq(m()(), 42);
var m = eval(evalStr1);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m()(), 42);
var evalStr2 = "(function() { " + body2 + "})";
var m = eval(evalStr2);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
assertEq(m()(), 43);
var m = eval(evalStr2);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m()(), 43);

var evalStr3 = "(function(global) { 'use asm'; var sin=global.Math.sin; function g(d) { d=+d; return +sin(d) } return g })";
var m = eval(evalStr3);
assertEq(isAsmJSModule(m), true);
assertEq(m(this)(.3), Math.sin(.3));
var m = eval(evalStr3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m(this)(.3), Math.sin(.3));
var evalStr4 = "(function(gobal) { 'use asm'; var sin=global.Math.sin; function g(d) { d=+d; return +sin(d) } return g })";
var m = eval(evalStr4);
assertEq(isAsmJSModule(m), false);
var m = eval(evalStr3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var evalStr5 = "(function(global,foreign) { 'use asm'; var sin=global.Math.sin; function g(d) { d=+d; return +sin(d) } return g })";
var m = eval(evalStr5);
assertEq(isAsmJSModuleLoadedFromCache(m), false);

var m = new Function(body1);
assertEq(isAsmJSModule(m), true);
var body3 = "'use asm'; var sin=global.Math.sin; function g(d) { d=+d; return +sin(d) } return g";
var m = new Function('global', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
assertEq(m(this)(.2), Math.sin(.2));
var m = new Function('gobal', body3);
assertEq(isAsmJSModule(m), false);
var m = new Function('global', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var m = new Function('global','foreign', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
var m = new Function('global','foreign', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var m = new Function('gobal','foreign', body3);
assertEq(isAsmJSModule(m), false);
var m = new Function('global','foreign', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var m = new Function('global','foregn', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
var m = new Function('global','foreign', 'buffer', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
var m = new Function('global','foreign', 'buffer', 'foopy', body3);
assertEq(isAsmJSModule(m), false);
var m = new Function('global','foreign', 'buffer', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var m = new Function('global','foreign', 'bffer', body3);
assertEq(isAsmJSModuleLoadedFromCache(m), false);
var m = new Function('global','foreign', 'foreign', body3);
assertEq(isAsmJSModule(m), false);

var body = "f() { 'use asm'; function g() {} return g }";
var evalStr6 = "(function " + body + ")";
var evalStr7 = "(function* " + body + ")";
var m = eval(evalStr6);
assertEq(isAsmJSModule(m), true);
var m = eval(evalStr6);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
var m = eval(evalStr7);
assertEq(isAsmJSModule(m), false);

// Test caching using a separate process (which, with ASLR, should mean a
// separate address space) to compile/cache the code. Ideally, every asmCompile
// would do this, but that makes jit-tests run 100x slower. Do it here, for one
// of each feature. asm.js/testBullet.js should pound on everything.
assertEq(asmLink(asmCompileCached(USE_ASM + "function f(i) { i=i|0; return +((i+1)|0) } function g(d) { d=+d; return +(d + +f(42) + 1.5) } return g"))(.2), .2+43+1.5);
assertEq(asmLink(asmCompileCached(USE_ASM + "function f1() { return 1 } function f2() { return 2 } function f(i) { i=i|0; return T[i&1]()|0 } var T=[f1,f2]; return f"))(1), 2);
assertEq(asmLink(asmCompileCached("g", USE_ASM + "var s=g.Math.sin; function f(d) { d=+d; return +s(d) } return f"), this)(.3), Math.sin(.3));
assertEq(asmLink(asmCompileCached("g","ffis", USE_ASM + "var ffi=ffis.ffi; function f(i) { i=i|0; return ffi(i|0)|0 } return f"), null, {ffi:function(i){return i+2}})(1), 3);
assertEq(asmLink(asmCompileCached("g","ffis", USE_ASM + "var x=ffis.x|0; function f() { return x|0 } return f"), null, {x:43})(), 43);
var i32 = new Int32Array(BUF_MIN/4);
i32[4] = 42;
assertEq(asmLink(asmCompileCached("g","ffis","buf", USE_ASM + "var i32=new g.Int32Array(buf); function f(i) { i=i|0; return i32[i>>2]|0 } return f"), this, null, i32.buffer)(4*4), 42);
assertEq(asmLink(asmCompileCached('glob', USE_ASM + 'var x=glob.Math.PI; function f() { return +x } return f'), this)(), Math.PI);
