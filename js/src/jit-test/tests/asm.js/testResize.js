load(libdir + "asm.js");

assertAsmTypeFail('glob', USE_ASM + "var I32=glob.Int32Arra; function f() {} return f");
var m = asmCompile('glob', USE_ASM + "var I32=glob.Int32Array; function f() {} return f");
assertAsmLinkFail(m, {});
assertAsmLinkFail(m, {Int32Array:null});
assertAsmLinkFail(m, {Int32Array:{}});
assertAsmLinkFail(m, {Int32Array:Uint32Array});
assertEq(asmLink(m, {Int32Array:Int32Array})(), undefined);
var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + "var I32=glob.Int32Array; function f() {} return f");
assertEq(asmLink(m, this)(), undefined);
assertEq(asmLink(m, this, null, BUF_64KB)(), undefined);

assertAsmTypeFail('glob', 'ffis', 'buf', USE_ASM + 'var I32=glob.Int32Array; var i32=new I3(buf); function f() {} return f');
assertAsmTypeFail('glob', 'ffis', 'buf', USE_ASM + 'var I32=0; var i32=new I32(buf); function f() {} return f');
var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var I32=glob.Int32Array; var i32=new I32(buf); function f() {} return f');
assertAsmLinkFail(m, this, null, {});
assertAsmLinkAlwaysFail(m, this, null, null);
assertAsmLinkFail(m, this, null, new ArrayBuffer(100));
assertEq(asmLink(m, this, null, BUF_64KB)(), undefined);

var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var I32=glob.Int32Array; var i32=new glob.Int32Array(buf); function f() {} return f');
assertAsmLinkFail(m, this, null, {});
assertAsmLinkAlwaysFail(m, this, null, null);
assertAsmLinkFail(m, this, null, new ArrayBuffer(100));
assertEq(asmLink(m, this, null, BUF_64KB)(), undefined);

var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var F32=glob.Float32Array; var i32=new glob.Int32Array(buf); function f() {} return f');
assertAsmLinkFail(m, this, null, {});
assertAsmLinkAlwaysFail(m, this, null, null);
assertAsmLinkFail(m, this, null, new ArrayBuffer(100));
assertEq(asmLink(m, this, null, BUF_64KB)(), undefined);
