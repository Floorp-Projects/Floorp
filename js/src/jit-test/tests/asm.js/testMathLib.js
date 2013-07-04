load(libdir + "asm.js");

function testUnary(f, g) {
    var numbers = [NaN, Infinity, -Infinity, -10000, -3.4, -0, 0, 3.4, 10000];
    for (n of numbers)
        assertEq(f(n), g(n));
}

var code = asmCompile('glob', USE_ASM + 'var sq=glob.Math.sin; function f(d) { d=+d; return +sq(d) } return f');
assertAsmLinkFail(code, {Math:{sin:Math.sqrt}});
assertAsmLinkFail(code, {Math:{sin:null}});
testUnary(asmLink(code, {Math:{sin:Math.sin}}), Math.sin);

var code = asmCompile('glob', USE_ASM + 'var co=glob.Math.cos; function f(d) { d=+d; return +co(d) } return f');
assertAsmLinkFail(code, {Math:{cos:Math.sqrt}});
assertAsmLinkFail(code, {Math:{cos:null}});
testUnary(asmLink(code, {Math:{cos:Math.cos}}), Math.cos);

var code = asmCompile('glob', USE_ASM + 'var ta=glob.Math.tan; function f(d) { d=+d; return +ta(d) } return f');
assertAsmLinkFail(code, {Math:{tan:Math.sqrt}});
assertAsmLinkFail(code, {Math:{tan:null}});
testUnary(asmLink(code, {Math:{tan:Math.tan}}), Math.tan);

var code = asmCompile('glob', USE_ASM + 'var as=glob.Math.asin; function f(d) { d=+d; return +as(d) } return f');
assertAsmLinkFail(code, {Math:{asin:Math.sqrt}});
assertAsmLinkFail(code, {Math:{asin:null}});
testUnary(asmLink(code, {Math:{asin:Math.asin}}), Math.asin);

var code = asmCompile('glob', USE_ASM + 'var ac=glob.Math.acos; function f(d) { d=+d; return +ac(d) } return f');
assertAsmLinkFail(code, {Math:{acos:Math.sqrt}});
assertAsmLinkFail(code, {Math:{acos:null}});
testUnary(asmLink(code, {Math:{acos:Math.acos}}), Math.acos);

var code = asmCompile('glob', USE_ASM + 'var at=glob.Math.atan; function f(d) { d=+d; return +at(d) } return f');
assertAsmLinkFail(code, {Math:{atan:Math.sqrt}});
assertAsmLinkFail(code, {Math:{atan:null}});
testUnary(asmLink(code, {Math:{atan:Math.atan}}), Math.atan);

var code = asmCompile('glob', USE_ASM + 'var ce=glob.Math.ceil; function f(d) { d=+d; return +ce(d) } return f');
assertAsmLinkFail(code, {Math:{ceil:Math.sqrt}});
assertAsmLinkFail(code, {Math:{ceil:null}});
testUnary(asmLink(code, {Math:{ceil:Math.ceil}}), Math.ceil);

var code = asmCompile('glob', USE_ASM + 'var fl=glob.Math.floor; function f(d) { d=+d; return +fl(d) } return f');
assertAsmLinkFail(code, {Math:{floor:Math.sqrt}});
assertAsmLinkFail(code, {Math:{floor:null}});
testUnary(asmLink(code, {Math:{floor:Math.floor}}), Math.floor);

var code = asmCompile('glob', USE_ASM + 'var exq=glob.Math.exp; function f(d) { d=+d; return +exq(d) } return f');
assertAsmLinkFail(code, {Math:{exp:Math.sqrt}});
assertAsmLinkFail(code, {Math:{exp:null}});
testUnary(asmLink(code, {Math:{exp:Math.exp}}), Math.exp);

var code = asmCompile('glob', USE_ASM + 'var lo=glob.Math.log; function f(d) { d=+d; return +lo(d) } return f');
assertAsmLinkFail(code, {Math:{log:Math.sqrt}});
assertAsmLinkFail(code, {Math:{log:null}});
testUnary(asmLink(code, {Math:{log:Math.log}}), Math.log);

var code = asmCompile('glob', USE_ASM + 'var sq=glob.Math.sqrt; function f(d) { d=+d; return +sq(d) } return f');
assertAsmLinkFail(code, {Math:{sqrt:Math.sin}});
assertAsmLinkFail(code, {Math:{sqrt:null}});
testUnary(asmLink(code, {Math:{sqrt:Math.sqrt}}), Math.sqrt);

var code = asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; return +abs(d) } return f');
assertAsmLinkFail(code, {Math:{abs:Math.sin}});
assertAsmLinkFail(code, {Math:{abs:null}});
testUnary(asmLink(code, {Math:{abs:Math.abs}}), Math.abs);

var f = asmLink(asmCompile('glob', USE_ASM + 'var abs=glob.Math.abs; function f(i) { i=i|0; return abs(i|0)|0 } return f'), this);
for (n of [-Math.pow(2,31)-1, -Math.pow(2,31), -Math.pow(2,31)+1, -1, 0, 1, Math.pow(2,31)-2, Math.pow(2,31)-1, Math.pow(2,31)])
    assertEq(f(n), Math.abs(n|0)|0);

function testBinary(f, g) {
    var numbers = [NaN, Infinity, -Infinity, -10000, -3.4, -0, 0, 3.4, 10000];
    for (n of numbers)
        for (o of numbers)
            assertEq(f(n,o), g(n,o));
}

var code = asmCompile('glob', USE_ASM + 'var po=glob.Math.pow; function f(d,e) { d=+d;e=+e; return +po(d,e) } return f');
assertAsmLinkFail(code, {Math:{pow:Math.sin}});
assertAsmLinkFail(code, {Math:{pow:null}});
testBinary(asmLink(code, {Math:{pow:Math.pow}}), Math.pow);

var code = asmCompile('glob', USE_ASM + 'var at=glob.Math.atan2; function f(d,e) { d=+d;e=+e; return +at(d,e) } return f');
assertAsmLinkFail(code, {Math:{atan2:Math.sin}});
assertAsmLinkFail(code, {Math:{atan2:null}});
testBinary(asmLink(code, {Math:{atan2:Math.atan2}}), Math.atan2);

assertAsmTypeFail('glob', USE_ASM + 'var sin=glob.Math.sin; function f(d) { d=+d; d = sin(d); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sin=glob.Math.sin; function f(d) { d=+d; var i=0; i = sin(d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pow=glob.Math.pow; function f(d) { d=+d; d = pow(d,d); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var pow=glob.Math.pow; function f(d) { d=+d; var i=0; i = pow(d,d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var atan2=glob.Math.atan2; function f(d) { d=+d; d = atan2(d,d); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var atan2=glob.Math.atan2; function f(d) { d=+d; var i=0; i = atan2(d,d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(d) { d=+d; d = sqrt(d); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var sqrt=glob.Math.sqrt; function f(d) { d=+d; sqrt(d)|0; } return f');
assertAsmTypeFail('glob', USE_ASM + 'var im=glob.Math.imul; function f(i) { i=i|0; var d=0.0; d = +im(i,i); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var im=glob.Math.imul; function f(i) { i=i|0; i = im(i,i); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var abs=glob.Math.abs; function f(i) { i=i|0; +abs(i|0); } return f');
assertAsmTypeFail('glob', USE_ASM + 'var abs=glob.Math.abs; function f(d) { d=+d; abs(d)|0; } return f');
