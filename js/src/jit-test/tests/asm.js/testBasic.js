load(libdir + "asm.js");
load(libdir + "asserts.js");

assertAsmTypeFail(USE_ASM);
assertAsmTypeFail(USE_ASM + 'return');
assertAsmTypeFail(USE_ASM + 'function f() 0');
assertAsmTypeFail(USE_ASM + 'function f(){}');
assertAsmTypeFail(USE_ASM + 'function f(){} return 0');
assertAsmTypeFail(USE_ASM + 'function f() 0; return 0');
assertAsmTypeFail(USE_ASM + 'function f(){} return g');
assertAsmTypeFail(USE_ASM + 'function f(){} function f(){} return f');
assertAsmTypeFail(USE_ASM + 'var f=0; function f(){} return f');
assertAsmTypeFail('glob', USE_ASM + 'var f=glob.Math.imul; function f(){} return f');
assertAsmTypeFail('glob','foreign', USE_ASM + 'var f=foreign.foo; function f(){} return f');
assertAsmTypeFail(USE_ASM + 'function f(){} var f=[f,f]; return f');
assertAsmTypeFail(USE_ASM + 'function f() 0; return f');
assertAsmTypeFail('"use strict";' + USE_ASM + 'function f() {} return f');
assertAsmTypeFail(USE_ASM + '"use strict"; function f() {} return f');
assertEq(asmLink(asmCompile(USE_ASM + 'function f(){} return f'))(), undefined);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(){;} return f'))(), undefined);
assertAsmTypeFail(USE_ASM + 'function f(i,j){;} return f');
assertEq(asmLink(asmCompile('"use asm";; function f(){};;; return f;;'))(), undefined);
assertAsmTypeFail(USE_ASM + 'function f(x){} return f');
assertAsmTypeFail(USE_ASM + 'function f(){return; return 1} return f');
assertEq(asmLink(asmCompile(USE_ASM + 'function f(x){x=x|0} return f'))(42), undefined);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(x){x=x|0; return x|0} return f'))(42), 42);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(x){x=x|0; return x|0;;;} return f'))(42), 42);
assertEq(asmLink(asmCompile(USE_ASM + 'function f(x,y){x=x|0;y=y|0; return (x+y)|0} return f'))(44, -2), 42);
assertAsmTypeFail('a', USE_ASM + 'function a(){} return a');
assertAsmTypeFail('a','b','c', USE_ASM + 'var c');
assertAsmTypeFail('a','b','c', USE_ASM + 'var c=0');
assertAsmTypeFail(USE_ASM + 'c=0;return {}');
assertAsmTypeFail('x','x', USE_ASM + 'function a(){} return a');
assertAsmTypeFail('x','y','x', USE_ASM + 'function a(){} return a');
assertEq(asmLink(asmCompile('x', USE_ASM + 'function a(){} return a'))(), undefined);
assertEq(asmLink(asmCompile('x','y', USE_ASM + 'function a(){} return a'))(), undefined);
assertEq(asmLink(asmCompile('x','y','z', USE_ASM + 'function a(){} return a'))(), undefined);
assertAsmTypeFail('x','y', USE_ASM + 'function y(){} return f');
assertEq(asmLink(asmCompile('x', USE_ASM + 'function f(){} return f'), 1, 2, 3)(), undefined);
assertEq(asmLink(asmCompile('x', USE_ASM + 'function f(){} return f'), 1)(), undefined);
assertEq(asmLink(asmCompile('x','y', USE_ASM + 'function f(){} return f'), 1, 2)(), undefined);

assertEq(asmLink(asmCompile(USE_ASM + 'function f(i) {i=i|0} return f'))(42), undefined);
assertEq(asmLink(asmCompile(USE_ASM + 'function f() {var i=0;; var j=1;} return f'))(), undefined); // bug 877965 second part
assertAsmTypeFail(USE_ASM + 'function f(i) {i=i|0;var i} return f');
assertAsmTypeFail(USE_ASM + 'function f(i) {i=i|0;var i=0} return f');

assertAsmTypeFail('glob', USE_ASM + 'var im=glob.imul; function f() {} return f');
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i,j) {i=i|0;j=j|0; return im(i,j)|0} return f'), null);
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i,j) {i=i|0;j=j|0; return im(i,j)|0} return f'), {});
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i,j) {i=i|0;j=j|0; return im(i,j)|0} return f'), {imul:Math.imul});
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i,j) {i=i|0;j=j|0; return im(i,j)|0} return f'), {Math:{imul:Math.imul}})(2,3), 6);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i,j) {i=i|0;j=j|0; return im(i,j)|0} return f'), this)(8,4), 32);

assertAsmLinkAlwaysFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), null, null);
assertAsmLinkAlwaysFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, null);
assertAsmLinkAlwaysFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, null);
assertAsmLinkAlwaysFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, new ArrayBuffer(1));
assertAsmLinkFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, new ArrayBuffer(100));
assertAsmLinkFail(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, new ArrayBuffer(4000));
assertEq(asmLink(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, new ArrayBuffer(4096))(), undefined);
assertEq(asmLink(asmCompile('glob','i','b', USE_ASM + 'var i32=new glob.Int32Array(b); function f(){} return f'), this, null, new ArrayBuffer(2*4096))(), undefined);

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i]|0; return i|0}; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>1]|0; return i|0}; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>1]|0; return i|0}; return f');
assertAsmLinkAlwaysFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>2]|0; return i|0}; return f'), this, null, new ArrayBuffer(4095));
assertEq(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>2]|0; return i|0}; return f')(this, null, new ArrayBuffer(4096))(), 0);

var exp = asmLink(asmCompile(USE_ASM + "return {}"));
assertEq(Object.keys(exp).length, 0);

var exp = asmLink(asmCompile(USE_ASM + "function f() { return 3 } return {f:f,f:f}"));
assertEq(exp.f(), 3);
assertEq(Object.keys(exp).join(), 'f');

assertAsmTypeFail(USE_ASM + "function f() { return 3 } return {1:f}");
assertAsmTypeFail(USE_ASM + "function f() { return 3 } return {__proto__:f}");
assertAsmTypeFail(USE_ASM + "function f() { return 3 } return {get x() {} }");

var exp = asmLink(asmCompile(USE_ASM + 'function internal() { return ((g()|0)+2)|0 } function f() { return 1 } function g() { return 2 } function h() { return internal()|0 } return {f:f,g1:g,h1:h}'));
assertEq(exp.f(), 1);
assertEq(exp.g1(), 2);
assertEq(exp.h1(), 4);
assertEq(Object.keys(exp).join(), 'f,g1,h1');

// can't test destructuring args with Function constructor
function assertTypeFailInEval(str)
{
    if (!isAsmJSCompilationAvailable())
        return;

    var caught = false;
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);
    try {
        eval(str);
    } catch (e) {
        assertEq((''+e).indexOf(ASM_TYPE_FAIL_STRING) == -1, false);
        caught = true;
    }
    assertEq(caught, true);
    options("werror");
}
assertTypeFailInEval('function f({}) { "use asm"; function g() {} return g }');
assertTypeFailInEval('function f({global}) { "use asm"; function g() {} return g }');
assertTypeFailInEval('function f(global, {imports}) { "use asm"; function g() {} return g }');
assertTypeFailInEval('function f(g = 2) { "use asm"; function g() {} return g }');
assertTypeFailInEval('function *f() { "use asm"; function g() {} return g }');

assertThrowsInstanceOf(function() { new Function(USE_ASM + 'var)') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'return)') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'var z=-2w') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'var z=-2w;') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f(') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f()') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f() {') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f() {} var') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f() {} var TBL=-2w; return f') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function f() {} var TBL=-2w return f') }, SyntaxError);
assertThrowsInstanceOf(function() { new Function(USE_ASM + 'function () {}') }, SyntaxError);
assertNoWarning(function() { parse("function f() { 'use asm'; function g() {} return g }") });
