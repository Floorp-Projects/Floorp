load(libdir + "asm.js");

assertAsmTypeFail(USE_ASM + "var i; function f(){} return f");
assertAsmTypeFail(USE_ASM + "const i; function f(){} return f");
assertEq(asmLink(asmCompile(USE_ASM + "var i=0; function f(){} return f"))(), undefined);
assertEq(asmLink(asmCompile(USE_ASM + "const i=0; function f(){} return f"))(), undefined);
assertEq(asmLink(asmCompile(USE_ASM + "var i=42; function f(){ return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "const i=42; function f(){ return i|0 } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "var i=4.2; function f(){ return +i } return f"))(), 4.2);
assertEq(asmLink(asmCompile(USE_ASM + "const i=4.2; function f(){ return +i } return f"))(), 4.2);
assertAsmTypeFail(USE_ASM + "var i=42; function f(){ return +(i+.1) } return f");
assertAsmTypeFail(USE_ASM + "const i=42; function f(){ return +(i+.1) } return f");
assertAsmTypeFail(USE_ASM + "var i=1.2; function f(){ return i|0 } return f");
assertAsmTypeFail(USE_ASM + "const i=1.2; function f(){ return i|0 } return f");
assertAsmTypeFail(USE_ASM + "var i=0; function f(e){ e=+e; i=e } return f");
assertAsmTypeFail(USE_ASM + "const i=0; function f(e){ e=+e; i=e } return f");
assertAsmTypeFail(USE_ASM + "var d=0.1; function f(i){ i=i|0; d=i } return f");
assertAsmTypeFail(USE_ASM + "const d=0.1; function f(i){ i=i|0; d=i } return f");
assertEq(asmLink(asmCompile(USE_ASM + "var i=13; function f(j) { j=j|0; i=j; return i|0 } return f"))(42), 42);
assertAsmTypeFail(USE_ASM + "const i=13; function f(j) { j=j|0; i=j; return i|0 } return f");
assertAsmTypeFail(USE_ASM + "const c=0,i=13; function f(j) { j=j|0; i=j; return i|0 } return f");
assertEq(asmLink(asmCompile(USE_ASM + "var d=.1; function f(e) { e=+e; d=e; return +e } return f"))(42.1), 42.1);
assertAsmTypeFail(USE_ASM + "const d=.1; function f(e) { e=+e; d=e; return +e } return f");
assertAsmTypeFail(USE_ASM + "const c=0, d=.1; function f(e) { e=+e; d=e; return +e } return f");
assertEq(asmLink(asmCompile(USE_ASM + "var i=13; function f(i, j) { i=i|0; j=j|0; i=j; return i|0 } return f"))(42,43), 43);
assertEq(asmLink(asmCompile(USE_ASM + "var i=13; function f(j) { j=j|0; var i=0; i=j; return i|0 } return f"))(42), 42);

var f = asmLink(asmCompile(USE_ASM + "var i=13; function f(j) { j=j|0; if ((j|0) != -1) { i=j } else { return i|0 } return 0 } return f"));
assertEq(f(-1), 13);
assertEq(f(42), 0);
assertEq(f(-1), 42);

assertAsmTypeFail('global', USE_ASM + "var i=global; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "const i=global; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "const i=global|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var j=0;var i=j.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "const i=global.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + "var i=global.i|0; function f() { return i|0 } return f");
assertAsmTypeFail('global', USE_ASM + 'var i=global.Infinity; function f() { i = 0.0 } return f');
assertAsmTypeFail('global', USE_ASM + 'const i=global.Infinity; function f() { i = 0.0 } return f');
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), undefined);
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'const i=global.Infinity; function f() { return +i } return f'), undefined);
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), null);
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'const i=global.Infinity; function f() { return +i } return f'), null);
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), 3);
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), {});
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), {Infinity:NaN});
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), {Infinity:-Infinity});
assertEq(asmLink(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), {Infinity:Infinity})(), Infinity);
assertEq(asmLink(asmCompile('global', USE_ASM + 'const i=global.Infinity; function f() { return +i } return f'), {Infinity:Infinity})(), Infinity);
assertEq(asmLink(asmCompile('global', USE_ASM + 'var i=global.Infinity; function f() { return +i } return f'), this)(), Infinity);
assertEq(asmLink(asmCompile('global', USE_ASM + 'const i=global.Infinity; function f() { return +i } return f'), this)(), Infinity);
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), undefined);
assertAsmLinkAlwaysFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), null);
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), 3);
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), {});
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), {Infinity:Infinity});
assertAsmLinkFail(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), {Infinity:-Infinity});
assertEq(asmLink(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), {NaN:NaN})(), NaN);
assertEq(asmLink(asmCompile('global', USE_ASM + 'const i=global.NaN; function f() { return +i } return f'), {NaN:NaN})(), NaN);
assertEq(asmLink(asmCompile('global', USE_ASM + 'var i=global.NaN; function f() { return +i } return f'), this)(), NaN);
assertEq(asmLink(asmCompile('global', USE_ASM + 'const i=global.NaN; function f() { return +i } return f'), this)(), NaN);

assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'var t = new glob.Int32Array(buf); function f() {} return f'), {get Int32Array(){return Int32Array}}, null, new ArrayBuffer(4096))
assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'const t = new glob.Int32Array(buf); function f() {} return f'), {get Int32Array(){return Int32Array}}, null, new ArrayBuffer(4096))
assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'var t = new glob.Int32Array(buf); function f() {} return f'), new Proxy({}, {get:function() {return Int32Array}}), null, new ArrayBuffer(4096))
assertAsmLinkFail(asmCompile('glob','foreign','buf', USE_ASM + 'const t = new glob.Int32Array(buf); function f() {} return f'), new Proxy({}, {get:function() {return Int32Array}}), null, new ArrayBuffer(4096))
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
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "const i=imp.i|0; function f() { return i|0 } return f")(null, {i:42})), 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=imp.i|0; function f() { return i|0 } return f")(null, {i:1.4})), 1);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "const i=imp.i|0; function f() { return i|0 } return f")(null, {i:1.4})), 1);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=+imp.i; function f() { return +i } return f")(null, {i:42})), 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "const i=+imp.i; function f() { return +i } return f")(null, {i:42})), 42);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "var i=+imp.i; function f() { return +i } return f")(this, {i:1.4})), 1.4);
assertEq(asmLink(asmCompile('global', 'imp', USE_ASM + "const i=+imp.i; function f() { return +i } return f")(this, {i:1.4})), 1.4);
assertEq(asmLink(asmCompile(USE_ASM + "var g=0; function f() { var i=42; while (1) { break; } g = i; return g|0 } return f"))(), 42);

var f1 = asmCompile('global', 'foreign', 'heap', USE_ASM + 'var i32 = new global.Int32Array(heap); function g() { return i32[4]|0 } return g');
var global = this;
var ab = new ArrayBuffer(4096);
var p = new Proxy(global,
                  {has:function(name) { f1(global, null, ab); return true},
                   getOwnPropertyDescriptor:function(name) { return {value:Int32Array}}});
new Int32Array(ab)[4] = 42;
assertEq(f1(p, null, ab)(), 42);

// GVN checks
assertEq(asmLink(asmCompile(USE_ASM + "var g=0; function f() { var x = 0; g = 1; x = g; return (x+g)|0 } return f"))(), 2);
assertEq(asmLink(asmCompile(USE_ASM + "var g=0; function f() { var x = 0; g = 1; x = g; g = 2; return (x+g)|0 } return f"))(), 3);
assertEq(asmLink(asmCompile(USE_ASM + "var g=0; var h=0; function f() { var x = 0; g = 1; x = g; h = 3; return (x+g)|0 } return f"))(), 2);
