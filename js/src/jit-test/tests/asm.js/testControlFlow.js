load(libdir + "asm.js");

assertAsmTypeFail(USE_ASM + "function f(i,j) { i=i|0;j=+j; if (i) return j; return j } return f");
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=+j; if (i) return j; return +~~i } return f"))(1,1.4), 1.4);
assertAsmTypeFail(USE_ASM + "function f(i,j) { i=i|0;j=j|0; if (i) return j^0; return i^0 } return f");
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; if (i) return j^0; return i|0 } return f"))(1,8), 8);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { while (0) {} return 0} return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { for (;0;) {} return 0} return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { do {} while(0); return 0} return f"))(), 0);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { while (0) ; return 0} return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { for (;0;) ; return 0} return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { do ; while(0); return 0} return f"))(), 0);

assertAsmTypeFail(USE_ASM + "function f(d) {d=+d; while (d) {}; return 0} return f");
assertAsmTypeFail(USE_ASM + "function f(d) {d=+d; for (;d;) {}; return 0} return f");
assertAsmTypeFail(USE_ASM + "function f(d) {d=+d; do {} while (d); return 0} return f");

assertEq(asmLink(asmCompile(USE_ASM + "function f(j) {j=j|0; var i=0; while ((i|0) < (j|0)) i=(i+4)|0; return i|0} return f"))(6), 8);
assertEq(asmLink(asmCompile(USE_ASM + "function f(j) {j=j|0; var i=0; for (;(i|0) < (j|0);) i=(i+4)|0; return i|0} return f"))(6), 8);
assertEq(asmLink(asmCompile(USE_ASM + "function f(j) {j=j|0; var i=0; do { i=(i+4)|0; } while ((i|0) < (j|0)); return i|0} return f"))(6), 8);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { while(1) return 42; return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { for(;1;) return 42; return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { do return 42; while(1); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while(1) { if (i) return 13; return 42 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;1;) { if (i) return 13; return 42 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { if (i) return 13; return 42 } while(1); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while(1) { break; while(1) {} } return 42 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;;) { break; for(;;) {} } return 42 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { break; do {} while(1) {} } while(1); return 42 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=1; while(1) { if (i) return 42; return 13 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=1; for(;1;) { if (i) return 42; return 13 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=1; do { if (i) return 42; return 13 } while(1); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while(1) { if (i) return 13; else return 42; return 13 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;;) { if (i) return 13; else return 42; return 13 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { if (i) return 13; else return 42; return 13 } while(1); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while((i|0) < 3) { if (i) return 42; i=(i+1)|0 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;(i|0) < 3;) { if (i) return 42; i=(i+1)|0 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { if (i) return 42; i=(i+1)|0 } while((i|0) < 3); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while((i|0) < 3) { if (!i) i=(i+1)|0; return 42 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;(i|0) < 3;) { if (!i) i=(i+1)|0; return 42 } return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { if (!i) i=(i+1)|0; return 42 } while((i|0) < 3); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42; return i|0; while(1) {} return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42; return i|0; for(;1;) {} return 0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42; return i|0; do {} while(1); return 0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while((i|0) < 10) if ((i|0) == 4) break; else i=(i+1)|0; return i|0 } return f"))(), 4);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(; (i|0) < 10;) if ((i|0) == 4) break; else i=(i+1)|0; return i|0 } return f"))(), 4);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do if ((i|0) == 4) break; else i=(i+1)|0; while((i|0) < 10); return i|0 } return f"))(), 4);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; while ((i=(i+1)|0)<2) { sum=(sum+1)|0; if ((i&1)==0) continue; sum=(sum+100)|0 } return sum|0 } return f"))(), 101);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; for (;(i=(i+1)|0)<2;) { sum=(sum+1)|0; if ((i&1)==0) continue; sum=(sum+100)|0 } return sum|0 } return f"))(), 101);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; do { sum=(sum+1)|0; if ((i&1)==0) continue; sum=(sum+100)|0 } while((i=(i+1)|0)<2); return sum|0 } return f"))(), 102);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; x:a:y:while(1) { i=1; while(1) { i=2; break a; } i=3; } return i|0 } return f"))(), 2);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; x:a:y:for(;;) { i=1; while(1) { i=2; break a; } i=3; } return i|0 } return f"))(), 2);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; x:a:y:do { i=1; while(1) { i=2; break a; } i=3; } while(1); return i|0 } return f"))(), 2);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; a:b:while((i|0) < 5) { i=(i+1)|0; while(1) continue b; } return i|0 } return f"))(), 5);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; a:b:for(;(i|0) < 5;) { i=(i+1)|0; while(1) continue b; } return i|0 } return f"))(), 5);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; a:b:do { i=(i+1)|0; while(1) continue b; } while((i|0) < 5); return i|0 } return f"))(), 5);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; return 0; a:b:while((i|0) < 5) { i=(i+1)|0; while(1) continue b; } return i|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; return 0; a:b:for(;(i|0) < 5;) { i=(i+1)|0; while(1) continue b; } return i|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; return 0; a:b:do { i=(i+1)|0; while(1) continue b; } while((i|0) < 5); return i|0 } return f"))(), 0);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42; a:{ break a; i=2; } b:{ c:{ break b; i=3 } i=4 } return i|0 } return f"))(), 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; a:b:for(;(i|0) < 5;i=(i+1)|0) { while(1) continue b; } return i|0 } return f"))(), 5);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42,sum=0; for(i=1;(i|0)<4;i=(i+1)|0) sum=(sum+i)|0; return sum|0 } return f"))(), 6);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42,sum=0; for(i=1;(i|0)<8;i=(i+1)|0) { if ((i&1) == 0) continue; sum=(sum+i)|0; } return sum|0 } return f"))(), 16);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while(1) { i=(i+1)|0; if ((i|0) > 10) break; } return i|0 } return f"))(), 11);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;1;i=(i+1)|0) { if ((i|0) > 10) break; } return i|0 } return f"))(), 11);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do { if ((i|0) > 10) break; i=(i+1)|0 } while(1); return i|0 } return f"))(), 11);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; while(1){ if ((i|0)>0) break; while (1) { i=i+1|0; if ((i|0)==1) break; } } return i|0; } return f"))(), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; for(;;){ if ((i|0)>0) break; while (1) { i=i+1|0; if ((i|0)==1) break; } } return i|0; } return f"))(), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; do{ if ((i|0)>0) break; while (1) { i=i+1|0; if ((i|0)==1) break; } }while(1); return i|0; } return f"))(), 1);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; while(1){ if ((i|0)>5) break; while (1) { i=i+1|0; sum=(sum+i)|0; if ((i|0)>3) break; } } return sum|0; } return f"))(), 21);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; for(;;){ if ((i|0)>5) break; while (1) { i=i+1|0; sum=(sum+i)|0; if ((i|0)>3) break; } } return sum|0; } return f"))(), 21);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0,sum=0; do{ if ((i|0)>5) break; while (1) { i=i+1|0; sum=(sum+i)|0; if ((i|0)>3) break; } }while(1); return sum|0; } return f"))(), 21);

assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; while(1) { if (i) { break; } else { return i|0 } i = 1 } return i|0 } return f"))(3), 3);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; for(;1;) { if (i) { break; } else { return i|0 } i = 1 } return i|0 } return f"))(3), 3);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; do { if (i) { break; } else { return i|0 } i = 1 } while (0); return i|0 } return f"))(3), 3);

assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; while(1) { if (i) { return i|0 } else { return i|0 } i = 1 } return i|0 } return f"))(3), 3);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; for(;;) { if (i) { return i|0 } else { return i|0 } i = 1 } return i|0 } return f"))(3), 3);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; do { if (i) { return i|0 } else { return i|0 } i = 1 } while (0); return i|0 } return f"))(3), 3);

assertEq(asmLink(asmCompile(USE_ASM + "function f() {var j=1,i=0; while(j){ if(0) continue; j=i } return j|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {var j=1,i=0; for(;j;){ if(0) continue; j=i } return j|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() {var j=1,i=0; do{ if(0) continue; j=i } while(j) return j|0 } return f"))(), 0);

assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; for(;;) { return i|0 } return 0 } return f"))(42), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f(n) { n=n|0; var i=0,s=0; for(;;i=(i+1)|0) { if (~~i==~~n) return s|0; s=(s+i)|0 } return 0 } return f"))(8), 28);

var f = asmLink(asmCompile(USE_ASM + "function f(n,m) { n=n|0;m=m|0; var i=0,sum=0; while((n|0)>(m|0) ? ((i|0)<(n|0))|0 : ((i|0)<(m|0))|0) { sum = (sum+i)|0; i=(i+1)|0 } return sum|0 } return f"));
assertEq(f(1,5), 10);
assertEq(f(6,5), 15);

var f = asmLink(asmCompile(USE_ASM + "function f(n,m) { n=n|0;m=m|0; var i=0,sum=0; for(; (n|0)>(m|0) ? ((i|0)<(n|0))|0 : ((i|0)<(m|0))|0; i=(i+1)|0) { sum = (sum+i)|0 } return sum|0 } return f"));
assertEq(f(1,5), 10);
assertEq(f(6,5), 15);

var f = asmLink(asmCompile(USE_ASM + "function f(n,m) { n=n|0;m=m|0; var i=0,sum=0; do { sum = (sum+i)|0; i=(i+1)|0 } while((n|0)>(m|0) ? ((i|0)<(n|0))|0 : ((i|0)<(m|0))|0); return sum|0 } return f"));
assertEq(f(1,5), 10);
assertEq(f(6,5), 15);

assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; switch(i|0) { case 1: return 0; case 1: return 0 } return 0} return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; switch(i|0) { case 1: return 0; case 2: return 0; case 1: return 0 } return 0} return f");
assertAsmTypeFail(USE_ASM + "function f(i) { i=i|0; switch(1) { case 1: return 0; case 1: return 0 } return 0} return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0; switch(i) {}; return i|0 } return f");
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) {}; return i|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { default: i=42 } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { default: i=42; break } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { case 0: i=42 } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { case 0: i=42; break } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { case 0: default: i=42 } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=0; switch(i|0) { case 0: default: i=42; break } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=1; switch(i|0) { case 0: case 2: break; default: i=42 } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=1; switch(i|0) { case 0: case 2: break; default: i=42; break } return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return 42; switch(1) { case 1: return 13 } return 14 } return f"))(), 42);

var exp = asmLink(asmCompile(USE_ASM + "var x=0; function a() { return x|0 } function b(i) { i=i|0; x=i } function c(i) { i=i|0; if (i) return b(i); } return {a:a,b:b,c:c}"));
assertEq(exp.c(10), undefined);
assertEq(exp.a(), 10);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; switch(i|0) { case 1: i=42; break; default: i=13 } return i|0 } return f"));
assertEq(f(-1), 13);
assertEq(f(0), 13);
assertEq(f(1), 42);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; switch(i|0) { case -1: i=42; break; default: i=13 } return i|0 } return f"));
assertEq(f(-1), 42);
assertEq(f(0), 13);
assertEq(f(1), 13);
assertEq(f(0xffffffff), 42);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var sum=0; switch(i|0) { case -1: sum=(sum+1)|0; case 1: sum=(sum+1)|0; case 3: sum=(sum+1)|0; default: sum=(sum+100)|0; } return sum|0 } return f"));
assertEq(f(-1), 103);
assertEq(f(0), 100);
assertEq(f(1), 102);
assertEq(f(2), 100);
assertEq(f(3), 101);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var sum=0; switch(i|0) { case -1: sum=10; break; case 1: sum=11; break; case 3: sum=12; break; default: sum=13; } return sum|0 } return f"));
assertEq(f(-1), 10);
assertEq(f(0), 13);
assertEq(f(1), 11);
assertEq(f(2), 13);
assertEq(f(3), 12);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=8,sum=0; a:for(; (i|0)<20; i=(i+1)|0) { switch(i&3) { case 0:case 1:sum=(sum+i)|0;break;case 2:sum=(sum+100)|0;continue;default:break a} sum=(sum+10)|0; } sum=(sum+1000)|0; return sum|0 } return f"))(), 1137);
assertEq(asmLink(asmCompile('g', USE_ASM + "function f() { g:{ return 42 } return 13 } return f"), null)(), 42);

var imp = { ffi:function() { throw "Wrong" } };
assertEq(asmLink(asmCompile('glob','imp', USE_ASM + "var ffi=imp.ffi; function f() { var i=0; return (i+1)|0; return ffi(i|0)|0 } return f"), null, imp)(), 1);
