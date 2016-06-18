load(libdir + "asm.js");

var f = asmLink(asmCompile(USE_ASM + "function a234567() { return 42 } return a234567"));
assertEq(f(), 42);
enableSPSProfiling();
assertEq(f(), 42);
