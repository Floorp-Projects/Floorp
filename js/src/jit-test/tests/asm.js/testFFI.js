load(libdir + "asm.js");
load(libdir + "asserts.js");

function ffi(a,b,c,d) {
    return a+b+c+d;
}

var f = asmLink(asmCompile('global','imp', USE_ASM + 'var ffi=imp.ffi; function g() { return 1 } function f() { var i=0; i=g()|0; return ((ffi(4,5,6,7)|0)+i)|0 } return f'), null, {ffi:ffi});
assertEq(f(1), 23);

var counter = 0;
function inc() { return counter++ }
function add1(x) { return x+1 }
function add2(x,y) { return x+y }
function add3(x,y,z) { return x+y+z }
function addN() {
    var sum = 0;
    for (var i = 0; i < arguments.length; i++)
        sum += arguments[i];
    return sum;
}
var imp = { inc:inc, add1:add1, add2:add2, add3:add3, addN:addN, identity: x => x };

assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { incc() } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { var i = 0; return (i + inc)|0 } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { inc = 0 } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return (inc() + 1)|0 } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return +((inc()|0) + 1.1) } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return +(inc() + 1.1) } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return (+inc() + 1)|0 } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { var i = 0; inc(i>>>0) } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return inc(); return } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { inc(inc()) } return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { g(inc()) } function g() {} return f');
assertAsmTypeFail('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { inc()|inc() } return f');

assertAsmLinkFail(asmCompile('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return inc()|0 } return f'), null, {});
assertAsmLinkFail(asmCompile('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return inc()|0 } return f'), null, {inc:0});
assertAsmLinkFail(asmCompile('glob', 'imp', USE_ASM + 'var inc=imp.inc; function f() { return inc()|0 } return f'), null, {inc:{}});

assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var inc=imp.inc; function g() { inc() } return g'), null, imp)(), undefined);
assertEq(counter, 1);

var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var inc=imp.inc; function g() { return inc()|0 } return g'), null, imp);
assertEq(f(), 1);
assertEq(counter, 2);
assertEq(f(), 2);
assertEq(counter, 3);

assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var add1=imp.add1; function g(i) { i=i|0; return add1(i|0)|0 } return g'), null, imp)(9), 10);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'const add1=imp.add1; function g(i) { i=i|0; return add1(i|0)|0 } return g'), null, imp)(9), 10);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var add3=imp.add3; function g() { var i=1,j=3,k=9; return add3(i|0,j|0,k|0)|0 } return g'), null, imp)(), 13);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'const add3=imp.add3; function g() { var i=1,j=3,k=9; return add3(i|0,j|0,k|0)|0 } return g'), null, imp)(), 13);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var add3=imp.add3; function g() { var i=1.4,j=2.3,k=32.1; return +add3(i,j,k) } return g'), null, imp)(), 1.4+2.3+32.1);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'const add3=imp.add3; function g() { var i=1.4,j=2.3,k=32.1; return +add3(i,j,k) } return g'), null, imp)(), 1.4+2.3+32.1);

assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var add3=imp.add3; function f(i,j,k) { i=i|0;j=+j;k=k|0; return add3(i|0,j,k|0)|0 } return f'), null, imp)(1, 2.5, 3), 6);
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var addN=imp.addN; function f() { return +addN(1,2,3,4.1,5,6.1,7,8.1,9.1,10,11.1,12,13,14.1,15.1,16.1,17.1,18.1) } return f'), null, imp)(), 1+2+3+4.1+5+6.1+7+8.1+9.1+10+11.1+12+13+14.1+15.1+16.1+17.1+18.1);

assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var add2=imp.add2; function f(i,j) { i=i|0;j=+j; return +(+(add2(i|0,1)|0) + +add2(j,1) + +add2(+~~i,j)) } return f'), null, imp)(2, 5.5), 3+(5.5+1)+(2+5.5));
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var addN=imp.addN; function f(i,j) { i=i|0;j=+j; return +(+addN(i|0,j,3,j,i|0) + +addN() + +addN(j,j,j)) } return f'), null, imp)(1, 2.2), (1+2.2+3+2.2+1)+(2.2+2.2+2.2));

counter = 0;
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var addN=imp.addN,inc=imp.inc; function f() { return ((addN(inc()|0,inc(3.3)|0,inc()|0)|0) + (addN(inc(0)|0)|0))|0 } return f'), null, imp)(), 6);
assertEq(counter, 4);

var recurse = function(i,j) { if (i == 0) return j; return f(i-1,j+1)+j }
imp.recurse = recurse;
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var r=imp.recurse; function f(i,j) { i=i|0;j=+j; return +r(i|0,j) } return f'), null, imp);
assertEq(f(0,3.3), 3.3);
assertEq(f(1,3.3), 3.3+4.3);
assertEq(f(2,3.3), 3.3+4.3+5.3);

function maybeThrow(i, j) {
    if (i == 0)
        throw j;
    try {
        return f(i-1, j);
    } catch(e) {
        assertEq(typeof e, "number");
        return e;
    }
}
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i, j) { i=i|0;j=j|0; return ffi(i|0, (j+1)|0)|0 } return f'), null, {ffi:maybeThrow});
assertThrowsValue(function() { f(0,0) }, 1);
assertThrowsValue(function() { f(0,Math.pow(2,31)-1) }, -Math.pow(2,31));
assertEq(f(1,0), 2);
assertEq(f(2,0), 3);
assertEq(f(3,0), 4);
assertEq(f(4,5), 10);

var recurse = function(i,j) { if (i == 0) throw j; f(i-1,j) }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function g(i,j,k) { i=i|0;j=+j;k=k|0; if (!(k|0)) ffi(i|0,j)|0; else g(i, j+1.0, (k-1)|0) } function f(i,j) { i=i|0;j=+j; g(i,j,4) } return f'), null, {ffi:recurse});
assertThrowsValue(function() { f(0,2.4) }, 2.4+4);
assertThrowsValue(function() { f(1,2.4) }, 2.4+8);
assertThrowsValue(function() { f(8,2.4) }, 2.4+36);

assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + 'var identity=imp.identity; function g(x) { x=+x; return +identity(x) } return g'), null, imp)(13.37), 13.37);

// Test asm.js => ion paths
setJitCompilerOption("ion.usecount.trigger", 10);
setJitCompilerOption("baseline.usecount.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);

// In registers on x64 and ARM, on the stack for x86
function ffiIntFew(a,b,c,d) { return d+1 }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=i|0; return ffi(i|0,(i+1)|0,(i+2)|0,(i+3)|0)|0 } return f'), null, {ffi:ffiIntFew});
for (var i = 0; i < 40; i++)
    assertEq(f(i), i+4);

// Stack and registers for x64 and ARM, stack for x86
function ffiIntMany(a,b,c,d,e,f,g,h,i,j) { return j+1 }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=i|0; return ffi(i|0,(i+1)|0,(i+2)|0,(i+3)|0,(i+4)|0,(i+5)|0,(i+6)|0,(i+7)|0,(i+8)|0,(i+9)|0)|0 } return f'), null, {ffi:ffiIntMany});
for (var i = 0; i < 15; i++)
    assertEq(f(i), i+10);

// In registers on x64 and ARM, on the stack for x86
function ffiDoubleFew(a,b,c,d) { return d+1 }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=+i; return +ffi(i,i+1.0,i+2.0,i+3.0) } return f'), null, {ffi:ffiDoubleFew});
for (var i = 0; i < 15; i++)
    assertEq(f(i), i+4);

// Stack and registers for x64 and ARM, stack for x86
function ffiDoubleMany(a,b,c,d,e,f,g,h,i,j) { return j+1 }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=+i; return +ffi(i,i+1.0,i+2.0,i+3.0,i+4.0,i+5.0,i+6.0,i+7.0,i+8.0,i+9.0) } return f'), null, {ffi:ffiDoubleMany});
for (var i = 0; i < 15; i++)
    assertEq(f(i), i+10);

// Test the throw path
function ffiThrow(n) { if (n == 14) throw 'yolo'; }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=i|0; ffi(i >> 0); } return f'), null, {ffi:ffiThrow});
var i = 0;
try {
    for (; i < 15; i++)
        f(i);
    throw 'assume unreachable';
} catch (e) {
    assertEq(e, 'yolo');
    assertEq(i, 14);
}

// OOL conversion paths
var INT32_MAX = Math.pow(2, 31) - 1;
function ffiOOLConvertInt(n) { if (n == 40) return valueToConvert; return 42; }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=i|0; return ffi(i >> 0) | 0; } return f'), null, {ffi:ffiOOLConvertInt});
for (var i = 0; i < 40; i++)
    assertEq(f(i), 42);
valueToConvert = INT32_MAX + 1;
assertEq(f(40), INT32_MAX + 1 | 0);
function testBadConversions(f) {
    valueToConvert = {valueOf: function () { throw "FAIL"; }};
    assertThrowsValue(() => f(40), "FAIL");
    valueToConvert = Symbol();
    assertThrowsInstanceOf(() => f(40), TypeError);
}
testBadConversions(f);

function ffiOOLConvertDouble(n) { if (n == 40) return valueToConvert; return 42.5; }
var f = asmLink(asmCompile('glob', 'imp', USE_ASM + 'var ffi=imp.ffi; function f(i) { i=i|0; return +ffi(i >> 0); } return f'), null, {ffi:ffiOOLConvertDouble});
for (var i = 0; i < 40; i++)
    assertEq(f(i), 42.5);
valueToConvert = {valueOf: function() { return 13.37 }};
assertEq(f(40), 13.37);
testBadConversions(f);
