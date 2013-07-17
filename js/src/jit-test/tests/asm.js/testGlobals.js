load(libdir + "asm.js");

assertAsmTypeFail(USE_ASM + "var i; function f(){} return f");
assertEq(asmLink(asmCompile(USE_ASM + "var i=0; function f(){} return f"))(), undefined);
assertEq(asmLink(asmCompile(USE_ASM + "var i=42; function f(){ return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "var i=4.2; function f(){ return +i } return f"))(), 4.2);
assertAsmTypeFail(USE_ASM + "var i=42; function f(){ return +(i+.1) } return f");
assertAsmTypeFail(USE_ASM + "var i=1.2; function f(){ return i|0 } return f");
assertAsmTypeFail(USE_ASM + "var i=0; function f(e){ e=+e; i=e } return f");
assertAsmTypeFail(USE_ASM + "var d=0.1; function f(i){ i=i|0; d=i } return f");
assertEq(asmLink(asmCompile(USE_ASM + "var i=13; function f(j) { j=j|0; i=j; return i|0 } return f"))(42), 42);
assertEq(asmLink(asmCompile(USE_ASM + "var d=.1; function f(e) { e=+e; d=e; return +e } return f"))(42.1), 42.1);

var f = asmLink(asmCompile(USE_ASM + "var i=13; function f(j) { j=j|0; if ((j|0) != -1) { i=j } else { return i|0 } return 0 } return f"));
assertEq(f(-1), 13);
assertEq(f(42), 0);
assertEq(f(-1), 42);

assertAsmTypeFail('global', USE_ASM + "var i=global; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var j=0;var i=j.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + 'var i=global.Infinity; function f() { i = 0.0 } return f');
var code = asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f');
assertAsmLinkAlwaysFail(code, undefined);
assertAsmLinkAlwaysFail(code, null);
assertAsmLinkFail(code, 3);
assertAsmLinkFail(code, {});
assertAsmLinkFail(code, {Infinity:NaN});
assertAsmLinkFail(code, {Infinity:-Infinity});
assertEq(asmLink(code, {Infinity:Infinity})(), Infinity);
var code = asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f');
assertEq(asmLink(code, this)(), Infinity);
var code = asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f');
assertAsmLinkAlwaysFail(code, undefined);
assertAsmLinkAlwaysFail(code, null);
assertAsmLinkFail(code, 3);
assertAsmLinkFail(code, {});
assertAsmLinkFail(code, {Infinity:Infinity});
assertAsmLinkFail(code, {Infinity:-Infinity});
assertEq(asmLink(code, {NaN:NaN})(), NaN);
var code = asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f');
assertEq(asmLink(code, this)(), NaN);

assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'var t = new glob.Int32Array(buf); function f() {} return f'), {get Int32Array(){return Int32Array}}, null, new ArrayBuffer(4096))
assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'var t = new glob.Int32Array(buf); function f() {} return f'), new Proxy({}, {get:function() {return Int32Array}}), null, new ArrayBuffer(4096))
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Math.sin; function f() {} return f'), {get Math(){return Math}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Math.sin; function f() {} return f'), new Proxy({}, {get:function(){return Math}}));
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Math.sin; function f() {} return f'), {Math:{get sin(){return Math.sin}}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Math.sin; function f() {} return f'), {Math:new Proxy({}, {get:function(){return Math.sin}})});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Infinity; function f() {} return f'), {get Infinity(){return Infinity}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + 'var t = glob.Math.sin; function f() {} return f'), new Proxy({}, {get:function(){return Infinity}}));
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = foreign.x|0; function f() {} return f'), null, {get x(){return 42}})
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = +foreign.x; function f() {} return f'), null, {get x(){return 42}})
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = foreign.x|0; function f() {} return f'), null, new Proxy({}, {get:function() { return 42}}));
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = +foreign.x; function f() {} return f'), null, new Proxy({}, {get:function() { return 42}}));
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = foreign.x; function f() {} return f'), null, {get x(){return function(){}}})
assertAsmLinkFail(asmCompile('glob','foreign', USE_ASM + 'var i = foreign.x; function f() {} return f'), null, new Proxy({}, {get:function() { return function(){}}}));
assertAsmTypeFail('global', 'imp', USE_ASM + "var i=imp; function f() { return i|0 } return f");
assertAsmTypeFail('global', 'imp', USE_ASM + "var j=0;var i=j.i|0; function f() { return i|0 } return f");
assertAsmLinkAlwaysFail(asmCompile('global','imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f"), null, undefined);
assertAsmLinkAlwaysFail(asmCompile('global','imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f"), this, undefined);
assertAsmLinkAlwaysFail(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f"), null, null);
assertAsmLinkAlwaysFail(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f"), this, null);
assertAsmLinkFail(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f"), this, 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f")(null, {i:42})), 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f")(null, {i:1.4})), 1);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=+imp.i; function f() { return +i } return f")(null, {i:42})), 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=+imp.i; function f() { return +i } return f")(this, {i:1.4})), 1.4);
assertEq(asmLink(asmCompile(USE_ASM + "var g=0; function f() { var i=42; while (1) { break; } g = i; return g|0 } return f"))(), 42);
