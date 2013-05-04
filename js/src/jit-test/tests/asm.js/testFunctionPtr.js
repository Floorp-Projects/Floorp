load(libdir + "asm.js");

assertAsmTypeFail('imp', USE_ASM + "function f() {} var imp=[f]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} var eval=[f]; return f");
assertAsmTypeFail(USE_ASM + "var tbl=0; function f() {} var tbl=[f]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} var tbl; return f");
assertAsmTypeFail(USE_ASM + "function f() {} var tbl=[]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} var tbl=[f,f,f]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} var tbl=[1]; return f");
assertAsmTypeFail(USE_ASM + "var g = 0; function f() {} var tbl=[g]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} function g(i) {i=i|0} var tbl=[f,g]; return f");
assertAsmTypeFail(USE_ASM + "function f() {} function g() {return 0} var tbl=[f,g]; return f");
assertAsmTypeFail(USE_ASM + "function f(i) {i=i|0} function g(i) {i=+i} var tbl=[f,g]; return f");
assertAsmTypeFail(USE_ASM + "function f() {return 0} function g() {return 0.0} var tbl=[f,g]; return f");
assertAsmTypeFail(USE_ASM + "var tbl=0; function g() {tbl[0&1]()} return g");
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return 42 } var tbl=[f]; return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 0} function g() {return 1} var tbl=[f,g]; return f"))(), 0);

assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return ([])[i&1]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return f[i&1]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return tbl[i]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return tbl[i&0]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return tbl[i&3]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i,j) { i=i|0;j=+j; return tbl[j&1]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return tbl[i&1](1)|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f(i) {i=i|0} function g(i) { i=i|0; return tbl[i&1]()|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f(i) {i=i|0} function g(i) { i=i|0; return tbl[i&1](3.0)|0 } var tbl=[f,f]; return g");
assertAsmTypeFail(USE_ASM + "function f(d) {d=+d} function g(i) { i=i|0; return tbl[i&1](3)|0 } var tbl=[f,f]; return g");
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g(i) { i=i|0; return tbl[i&1]()|0 } var tbl=[f,f]; return g"))(0), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g() {return 13} function h(i) { i=i|0; return tbl[i&1]()|0 } var tbl=[f,g]; return h"))(1), 13);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g() {return 13} function h(i) { i=i|0; return tbl2[i&1]()|0 } var tbl1=[f,g]; var tbl2=[g,f]; return h"))(1), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g() {return 13} function h(i) { i=i|0; return tbl2[i&3]()|0 } var tbl1=[f,g]; var tbl2=[g,g,g,f]; return h"))(3), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g() {return 13} function h(i) { i=i|0; return tbl1[i&1]()|0 } var tbl1=[f,g]; var tbl2=[g,g,g,f]; return h"))(1), 13);
assertEq(asmLink(asmCompile(USE_ASM + "var i=0,j=0; function f() {return i|0} function g() {return j|0} function h(x) { x=x|0; i=5;j=10; return tbl2[x&3]()|0 } var tbl1=[f,g]; var tbl2=[g,g,g,f]; return h"))(3), 5);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + "var ffi=imp.ffi; function f() {return ffi()|0} function g() {return 13} function h(x) { x=x|0; return tbl2[x&3]()|0 } var tbl2=[g,g,g,f]; return h"), null, {ffi:function(){return 20}})(3), 20);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + "var ffi=imp.ffi; var i=0; function f() {return (~~ffi()+i)|0} function g() {return 13} function h(x) { x=x|0; i=2; return tbl2[x&3]()|0 } var tbl2=[g,g,g,f]; return h"), null, {ffi:function(){return 20}})(3), 22);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) {i=i|0; return +((i+1)|0)} function g(d) { d=+d; return +(d+2.5) } function h(i,j) { i=i|0;j=j|0; return +tbl2[i&1](+tbl1[i&1](j)) } var tbl1=[f,f]; var tbl2=[g,g]; return h"))(0,10), 11+2.5);

assertAsmTypeFail(USE_ASM + "function f() {return 42} function g() { return tbl[0]()|0 } var tbl=[f]; return g");
assertEq(asmLink(asmCompile(USE_ASM + "function f() {return 42} function g() { return tbl[0&0]()|0 } var tbl=[f]; return g"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f1() {return 42} function f2() {return 13} function g() { return tbl[1&1]()|0 } var tbl=[f1,f2]; return g"))(), 13);

var f = asmLink(asmCompile(USE_ASM + "function f1(d) {d=+d; return +(d/2.0)} function f2(d) {d=+d; return +(d+10.0)} function g(i,j) { i=i|0;j=+j; return +tbl[i&1](+tbl[(i+1)&1](j)) } var tbl=[f1,f2]; return g"));
assertEq(f(0,10.2), (10.2+10)/2);
assertEq(f(1,10.2), (10.2/2)+10);

var f = asmLink(asmCompile('glob','imp', USE_ASM + "var ffi=imp.ffi; function f(){return 13} function g(){return 42} function h(i) { i=i|0; var j=0; ffi(1); j=TBL[i&7]()|0; ffi(1.5); return j|0 } var TBL=[f,g,f,f,f,f,f,f]; return h"), null, {ffi:function(){}});
for (var i = 0; i < 100; i++)
    assertEq(f(i), (i%8 == 1) ? 42 : 13);
