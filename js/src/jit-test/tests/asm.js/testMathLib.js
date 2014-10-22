load(libdir + "asm.js");

function testUnary(f, g) {
    var numbers = [NaN, Infinity, -Infinity, -10000, -3.4, -0, 0, 3.4, 10000];
    for (n of numbers)
        assertEq(f(n), g(n));
}

const FROUND = 'var fround=glob.Math.fround;';

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sin; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sin:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sin; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sin:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sin; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sin:Math.sin}}), Math.sin);
testUnary(asmLink(asmCompile('glob', USE_ASM + 'const sq=glob.Math.sin; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sin:Math.sin}}), Math.sin);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var co=glob.Math.cos; function f(d) { d=+d; return +co(d) } return f'), {Math:{cos:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var co=glob.Math.cos; function f(d) { d=+d; return +co(d) } return f'), {Math:{cos:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var co=glob.Math.cos; function f(d) { d=+d; return +co(d) } return f'), {Math:{cos:Math.cos}}), Math.cos);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ta=glob.Math.tan; function f(d) { d=+d; return +ta(d) } return f'), {Math:{tan:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ta=glob.Math.tan; function f(d) { d=+d; return +ta(d) } return f'), {Math:{tan:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var ta=glob.Math.tan; function f(d) { d=+d; return +ta(d) } return f'), {Math:{tan:Math.tan}}), Math.tan);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var as=glob.Math.asin; function f(d) { d=+d; return +as(d) } return f'), {Math:{asin:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var as=glob.Math.asin; function f(d) { d=+d; return +as(d) } return f'), {Math:{asin:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var as=glob.Math.asin; function f(d) { d=+d; return +as(d) } return f'), {Math:{asin:Math.asin}}), Math.asin);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ac=glob.Math.acos; function f(d) { d=+d; return +ac(d) } return f'), {Math:{acos:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ac=glob.Math.acos; function f(d) { d=+d; return +ac(d) } return f'), {Math:{acos:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var ac=glob.Math.acos; function f(d) { d=+d; return +ac(d) } return f'), {Math:{acos:Math.acos}}), Math.acos);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan; function f(d) { d=+d; return +at(d) } return f'), {Math:{atan:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan; function f(d) { d=+d; return +at(d) } return f'), {Math:{atan:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan; function f(d) { d=+d; return +at(d) } return f'), {Math:{atan:Math.atan}}), Math.atan);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ce=glob.Math.ceil; function f(d) { d=+d; return +ce(d) } return f'), {Math:{ceil:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var ce=glob.Math.ceil; function f(d) { d=+d; return +ce(d) } return f'), {Math:{ceil:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var ce=glob.Math.ceil; function f(d) { d=+d; return +ce(d) } return f'), {Math:{ceil:Math.ceil}}), Math.ceil);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var fl=glob.Math.floor; function f(d) { d=+d; return +fl(d) } return f'), {Math:{floor:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var fl=glob.Math.floor; function f(d) { d=+d; return +fl(d) } return f'), {Math:{floor:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var fl=glob.Math.floor; function f(d) { d=+d; return +fl(d) } return f'), {Math:{floor:Math.floor}}), Math.floor);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var exq=glob.Math.exp; function f(d) { d=+d; return +exq(d) } return f'), {Math:{exp:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var exq=glob.Math.exp; function f(d) { d=+d; return +exq(d) } return f'), {Math:{exp:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var exq=glob.Math.exp; function f(d) { d=+d; return +exq(d) } return f'), {Math:{exp:Math.exp}}), Math.exp);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var lo=glob.Math.log; function f(d) { d=+d; return +lo(d) } return f'), {Math:{log:Math.sqrt}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var lo=glob.Math.log; function f(d) { d=+d; return +lo(d) } return f'), {Math:{log:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var lo=glob.Math.log; function f(d) { d=+d; return +lo(d) } return f'), {Math:{log:Math.log}}), Math.log);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sqrt; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sqrt:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sqrt; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sqrt:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var sq=glob.Math.sqrt; function f(d) { d=+d; return +sq(d) } return f'), {Math:{sqrt:Math.sqrt}}), Math.sqrt);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; return +abs(d) } return f'), {Math:{abs:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; return +abs(d) } return f'), {Math:{abs:null}});
testUnary(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; return +abs(d) } return f'), {Math:{abs:Math.abs}}), Math.abs);

var f = asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(i) { i=i|0; return abs(i|0)|0 } return f'), this);
for (n of [-Math.pow(2,31)-1, -Math.pow(2,31), -Math.pow(2,31)+1, -1, 0, 1, Math.pow(2,31)-2, Math.pow(2,31)-1, Math.pow(2,31)])
    assertEq(f(n), Math.abs(n|0)|0);

var f = asmLink(asmCompile('glob', USE_ASM + 'var clz32=glob.Math.clz32; function f(i) { i=i|0; return clz32(i)|0 } return f'), this);
for (n of [0, 1, 2, 15, 16, Math.pow(2,31)-1, Math.pow(2,31), Math.pow(2,31)+1, Math.pow(2,32)-1, Math.pow(2,32), Math.pow(2,32)+1])
    assertEq(f(n), Math.clz32(n|0));

var doubleNumbers = [NaN, Infinity, -Infinity, -10000, -3.4, -0, 0, 3.4, 10000];
var floatNumbers = [];
for (var x of doubleNumbers) floatNumbers.push(Math.fround(x));
var intNumbers = [-Math.pow(2,31), -10000, -3, -1, 0, 3, 10000, Math.pow(2,31)];

function testBinary(f, g, numbers) {
    for (n of numbers)
        for (o of numbers)
            assertEq(f(n,o), g(n,o));
}

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var po=glob.Math.pow; function f(d,e) { d=+d;e=+e; return +po(d,e) } return f'), {Math:{pow:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var po=glob.Math.pow; function f(d,e) { d=+d;e=+e; return +po(d,e) } return f'), {Math:{pow:null}});
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var po=glob.Math.pow; function f(d,e) { d=+d;e=+e; return +po(d,e) } return f'), {Math:{pow:Math.pow}}), Math.pow, doubleNumbers);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan2; function f(d,e) { d=+d;e=+e; return +at(d,e) } return f'), {Math:{atan2:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan2; function f(d,e) { d=+d;e=+e; return +at(d,e) } return f'), {Math:{atan2:null}});
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var at=glob.Math.atan2; function f(d,e) { d=+d;e=+e; return +at(d,e) } return f'), {Math:{atan2:Math.atan2}}), Math.atan2, doubleNumbers);

function coercedMin(...args) { for (var i = 0; i < args.length; i++) args[i] = args[i]|0; return Math.min(...args) }
function coercedMax(...args) { for (var i = 0; i < args.length; i++) args[i] = args[i]|0; return Math.max(...args) }
assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(d) { d=+d; return +min(d) } return f');
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + 'var i32=new glob.Int32Array(heap); var min=glob.Math.min; function f() { return min(i32[0], 5)|0 } return f');
assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(x) { x=x|0; return min(3 + x, 5)|0 } return f');
assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(x) { x=x|0; return min(5, 3 + x)|0 } return f');
assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(x) { x=x|0; return min(x, 1)|0 } return f');

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e) { d=+d;e=+e; return +min(d,e) } return f'), {Math:{min:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e) { d=+d;e=+e; return +min(d,e) } return f'), {Math:{min:null}});
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e) { d=+d;e=+e; return +min(d,e) } return f'), {Math:{min:Math.min}}), Math.min, doubleNumbers);
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; var f32=glob.Math.fround; function f(d,e) { d=f32(d);e=f32(e); return f32(min(d,e)) } return f'), this), Math.min, floatNumbers);
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e) { d=d|0;e=e|0; return min(d|0,e|0)|0} return f'), {Math:{min:Math.min}}), coercedMin, intNumbers);

assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e) { d=+d;e=+e; return +max(d,e) } return f'), {Math:{max:Math.sin}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e) { d=+d;e=+e; return +max(d,e) } return f'), {Math:{max:null}});
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e) { d=+d;e=+e; return +max(d,e) } return f'), {Math:{max:Math.max}}), Math.max, doubleNumbers);
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; var f32=glob.Math.fround; function f(d,e) { d=f32(d);e=f32(e); return f32(max(d,e)) } return f'), this), Math.max, floatNumbers);
testBinary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e) { d=d|0;e=e|0; return max(d|0,e|0)|0} return f'), {Math:{max:Math.max}}), coercedMax, intNumbers);

function testTernary(f, g, numbers) {
    for (n of numbers)
        for (o of numbers)
            for (p of numbers)
                assertEq(f(n,o,p), g(n,o,p));
}

assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e,g) { d=+d;e=+e;g=g|0; return +min(d,e,g) } return f');
assertAsmTypeFail('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e,g) { d=d|0;e=e|0;g=+g; return max(d,e,g)|0 } return f');
assertAsmTypeFail('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e,g) { d=+d;e=+e;g=+g; return min(d,e,g)|0 } return f');
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e,g) { d=d|0;e=e|0;g=g|0; return +max(d|0,e|0,g|0) } return f'), {Math:{max:Math.max}}), coercedMax, intNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e,g) { d=d|0;e=e|0;g=g|0; return max(d|0,e|0,g|0)|0 } return f'), {Math:{max:Math.max}}), coercedMax, intNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d,e,g) { d=+d;e=+e;g=+g; return +max(d,e,g) } return f'), {Math:{max:Math.max}}), Math.max, doubleNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; var _=glob.Math.fround; function f(d,e,g) { d=_(d);e=_(e);g=_(g); return _(max(d,e,g)) } return f'), this), Math.max, floatNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e,g) { d=d|0;e=e|0;g=g|0; return min(d|0,e|0,g|0)|0 } return f'), {Math:{min:Math.min}}), coercedMin, intNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d,e,g) { d=+d;e=+e;g=+g; return +min(d,e,g) } return f'), {Math:{min:Math.min}}), Math.min, doubleNumbers);
testTernary(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; var _=glob.Math.fround; function f(d,e,g) { d=_(d);e=_(e);g=_(g); return _(min(d,e,g)) } return f'), this), Math.min, floatNumbers);

// Implicit return coercions of math functions
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i) { i=i|0; i = im(i,i); return i|0 } return f'), this)(3), 9);
assertAsmTypeFail('glob', USE_ASM + 'var im=glob.Math.imul; function f(d) { d=+d; d = im(d, d) } return f');
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var im=glob.Math.imul; function f(d) { d=fround(d); d = im(d, d) } return f');

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=d|0; d = abs(d|0); return +(d>>>0) } return f'), this)(-1), 1);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=d|0; var m = 0; m = (-1)>>>0; return (abs(d|0) < (m>>>0))|0 } return f'), this)(42), 1);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; d = abs(d); return +d } return f'), this)(-1.5), 1.5);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var abs=glob.Math.abs; function f(d) { d=fround(d); d = fround(abs(d)); return +d } return f'), this)(-1.5), 1.5);
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var abs=glob.Math.abs; function f(d) { d=fround(d); d = abs(d); return +d } return f');
assertAsmTypeFail('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=d|0; var m = 0; m = -1; return (abs(d|0) < (m|0))|0 } return f');

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(d) { d=+d; d = sqrt(d); return +d } return f'), this)(256), 16);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var sqrt=glob.Math.sqrt; function f(d) { d=fround(d); d = fround(sqrt(d)); return +d } return f'), this)(13.37), Math.fround(Math.sqrt(Math.fround(13.37))));
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var sqrt=glob.Math.sqrt; function f(d) { d=fround(d); d = sqrt(d); return fround(d) } return f');
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var sqrt=glob.Math.sqrt; function f(d) { d=fround(d); d = sqrt(d); return d } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(n) { n=n|0; var d=0.; d = sqrt(n|0) } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(n) { n=n|0; var d=3.; n = sqrt(d)|0 } return f');

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d) { d=+d; d = min(d, 13.); return +d } return f'), this)(12), 12);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d) { d=d|0; d = min(d|0, 11); return d|0 } return f'), this)(12), 11);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var min=glob.Math.min; function f(d) { d=fround(d); d = min(d, fround(13.37)); return fround(d) } return f'), this)(14), Math.fround(13.37));

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var sin=glob.Math.sin; function f(d) { d=+d; d = sin(d); return +d } return f'), this)(Math.PI), Math.sin(Math.PI));
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var sin=glob.Math.sin; function f(d) { d=fround(d); d = sin(d) } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sin=glob.Math.sin; function f(d) { d=d|0; d = sin(d) } return f');

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var pow=glob.Math.pow; function f(d) { d=+d; d = pow(d,d); return +d } return f'), this)(3), 27);
assertAsmTypeFail('glob', USE_ASM + FROUND + 'var pow=glob.Math.pow; function f(d) { d=fround(d); d = pow(d, d) } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pow=glob.Math.pow; function f(d) { d=d|0; d = pow(d, d) } return f');

assertAsmTypeFail('glob', USE_ASM + 'var sin=glob.Math.sin; function f(d) { d=+d; var i=0; i = sin(d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pow=glob.Math.pow; function f(d) { d=+d; var i=0; i = pow(d,d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var atan2=glob.Math.atan2; function f(d) { d=+d; var i=0; i = atan2(d,d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(d) { d=+d; sqrt(d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; abs(d)|0; } return f');

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var im=glob.Math.imul; function f(i) { i=i|0; var d=0.0; d = +im(i,i); return +d } return f'), this)(42), Math.imul(42, 42));
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(i) { i=i|0; var d=0.0; d = +abs(i|0); return +d } return f'), this)(-42), 42);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(i) { i=i|0; var d=0.0; d = +min(i|0, 0); return +d } return f'), this)(-42), -42);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var min=glob.Math.min; function f(i) { i=i|0; var d=fround(0); d = fround(min(i|0, 0)); return +d } return f'), this)(-42), -42);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(i) { i=i|0; var d=0.0; d = +max(i|0, 0); return +d } return f'), this)(-42), 0);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var max=glob.Math.max; function f(i) { i=i|0; var d=fround(0); d = fround(max(i|0, 0)); return +d } return f'), this)(-42), 0);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var min=glob.Math.min; function f(d) { d=+d; var i=0; i = ~~min(d, 0.)|0; return i|0 } return f'), this)(-42), -42);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var min=glob.Math.min; function f(d) { d=fround(d); var i=0; i = ~~min(d, fround(0))|0; return i|0 } return f'), this)(-42), -42);
assertEq(asmLink(asmCompile('glob', USE_ASM + 'var max=glob.Math.max; function f(d) { d=+d; var i=0; i = ~~max(d, 0.)|0; return i|0 } return f'), this)(-42), 0);
assertEq(asmLink(asmCompile('glob', USE_ASM + FROUND + 'var max=glob.Math.max; function f(d) { d=fround(d); var i=0; i = ~~max(d, fround(0))|0; return i|0 } return f'), this)(-42), 0);

assertEq(asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(i) { i=i|0; var d=0.0; return +d; +abs(i|0); return 3.0;} return f'), this)(-42), 0);

assertAsmTypeFail('glob', USE_ASM + 'var tau=glob.Math.TAU; function f() {} return f');
assertAsmTypeFail('glob', USE_ASM + 'var pi=glob.Math.PI; function f() { return pi | 0 } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pi=glob.Math.PI; function f() { return +pi() } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pi=glob.Math.PI; function f() { pi = +3; } return f');
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + 'var pi=glob.Math.PI; function f() {} return f'), {});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var pi=glob.Math.PI; function f() {} return f'), {Math: {}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var pi=glob.Math.PI; function f() {} return f'), {Math: {PI: Math.cos}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var pi=glob.Math.PI; function f() {} return f'), {Math: {PI: Math.SQRT2}});

for (var c of ['E', 'LN10', 'LN2', 'LOG2E', 'LOG10E', 'PI', 'SQRT1_2', 'SQRT2']) {
    var f = asmLink(asmCompile('glob', USE_ASM + 'var x=glob.Math.' + c +'; function f() { return +x } return f'), this);
    assertEq(f(), eval('Math.' + c));
}
