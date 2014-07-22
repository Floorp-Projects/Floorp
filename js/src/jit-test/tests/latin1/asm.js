load(libdir + "asm.js");

if (!isAsmJSCompilationAvailable() || !isCachingEnabled())
    quit();

// Test Latin1 and TwoByte PropertyName serialization.

// Latin1
var body1 = "'use asm'; function funName() { return 42 } return funName";
var m = new Function(body1);
assertEq(isAsmJSModule(m), true);
assertEq(m()(), 42);
var m = new Function(body1);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m()(), 42);

var f = m();
assertEq(isLatin1(f.name), true);
assertEq(f.name, "funName");

// TwoByte
var body1 = "'use asm'; function funName\u1200() { return 42 } return funName\u1200";
var m = new Function(body1);
assertEq(isAsmJSModule(m), true);
assertEq(m()(), 42);
var m = new Function(body1);
assertEq(isAsmJSModuleLoadedFromCache(m), true);
assertEq(m()(), 42);

var f = m();
assertEq(isLatin1(f.name), false);
assertEq(f.name, "funName\u1200");
