load(libdir + "asm.js");

assertAsmTypeFail(USE_ASM + "function f(i,j,k) { i=i|0;j=+j;k=+k; return (i+(j+k))|0 } return f");
assertAsmTypeFail(USE_ASM + "function f(i,j,k) { i=i|0;j=j|0;k=+k; return +((i+j)+k) } return f");
assertAsmTypeFail('imp', USE_ASM + "var ffi=imp.ffi; function f(i) { i=i|0; return (i+ffi())|0 } return f");

assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j,k) { i=i|0;j=j|0;k=k|0; return (i+j+k)|0 } return f"))(1,2,3), 6);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j,k) { i=i|0;j=j|0;k=k|0; return (i+j-k)|0 } return f"))(1,2,3), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j,k) { i=i|0;j=j|0;k=k|0; return (i-j+k)|0 } return f"))(1,2,3), 2);

const INT32_MAX = Math.pow(2,31)-1;
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (i+i+i+i+i+i+i+i+i+i)|0 } return f"))(INT32_MAX), (10*INT32_MAX)|0);
const INT32_MIN = -Math.pow(2,31);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (i+i+i+i+i+i+i+i+i+i)|0 } return f"))(INT32_MIN), (10*INT32_MIN)|0);
