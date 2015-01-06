load(libdir + "asserts.js");

if (helperThreadCount() === 0)
  quit(0);

options('werror');
offThreadCompileScript("function f() {'use asm'}");
assertThrowsInstanceOf(()=>runOffThreadScript(), TypeError);
