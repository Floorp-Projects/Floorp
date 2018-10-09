// |jit-test| skip-if: helperThreadCount() === 0

load(libdir + "asserts.js");

options('werror');
offThreadCompileScript("function f() {'use asm'}");
assertThrowsInstanceOf(()=>runOffThreadScript(), TypeError);
