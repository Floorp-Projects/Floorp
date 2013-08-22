load(libdir + 'asm.js');

assertAsmTypeFail(USE_ASM + 'function f(d) { d=+d; var e=0; e=d; return +e } return f');
assertAsmTypeFail(USE_ASM + 'function f(d) { d=+d; var e=1e1; e=d; return +e } return f');
assertAsmTypeFail(USE_ASM + 'function f(d) { d=+d; var e=+0; e=d; return +e } return f');
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { var e=-0; return +e } return f'))(-0), -0);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { var e=-0.0; return +e } return f'))(-0), -0);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=0.0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=-0.0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=10.0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=-10.0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=1.0e2; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=-1.0e2; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=1.0e0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(d) { d=+d; var e=-1.0e0; e=d; return +e } return f'))(0.1), 0.1);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { return 0.0 } function g() { var d=0.1; d=+f(); return +d } return g'))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { return -0.0 } function g() { var d=0.1; d=+f(); return +d } return g'))(), -0);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { return 10.0 } function g() { var d=0.1; d=+f(); return +d } return g'))(), 10);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { return -10.0 } function g() { var d=0.1; d=+f(); return +d } return g'))(), -10.0);

assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=1e10; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=9007199254740991e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=9007199254740992e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=9007199254740993e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=-9007199254740991e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=-9007199254740992e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=-9007199254740993e0; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=1e100; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=1e10000; j=i; return j|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; var j=1000000000000000000; j=i; return j|0 } return f");
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var j=1e0; j=i; return j|0 } return f"))(42), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var j=1e9; j=i; return j|0 } return f"))(42), 42);

assertAsmTypeFail(USE_ASM + 'function f() { var i=-2147483649; return i|0 } return f');
assertAsmTypeFail(USE_ASM + 'function f() { var i=4294967296; return i|0 } return f');
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { var i=-2147483648; return i|0 } return f'))(), -2147483648);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() { var i=4294967295; return i|0 } return f'))(), 4294967295|0);
assertAsmTypeFail(USE_ASM + 'function f(i) { i=i|0; return (i+-2147483649)|0 } return f');
assertAsmTypeFail(USE_ASM + 'function f(i) { i=i|0; return (i+4294967296)|0 } return f');
assertEq(asmLink(asmCompile(USE_ASM + 'function f(i) { i=i|0; return (i+-2147483648)|0 } return f'))(0), -2147483648);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(i) { i=i|0; return (i+4294967295)|0 } return f'))(0), 4294967295|0);

assertAsmTypeFail(USE_ASM + 'var i=-2147483649; function f() { return i|0 } return f');
assertAsmTypeFail(USE_ASM + 'var i=4294967296; function f() { return i|0 } return f');
assertEq(asmLink(asmCompile(USE_ASM + 'var i=-2147483648; function f() { return i|0 } return f'))(), -2147483648);
assertEq(asmLink(asmCompile(USE_ASM + 'var i=4294967295; function f() { return i|0 } return f'))(), 4294967295|0);
