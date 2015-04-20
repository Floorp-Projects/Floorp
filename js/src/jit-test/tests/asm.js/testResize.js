// |jit-test| test-also-noasmjs
load(libdir + "asm.js");
load(libdir + "asserts.js");

// Tests for importing typed array view constructors

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

// Tests for link-time validation of byteLength import

assertAsmTypeFail('glob', 'ffis', 'buf', USE_ASM + 'var byteLength=glob.byteLength; function f() { return byteLength(1)|0 } return f');

var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var byteLength=glob.byteLength; function f() { return 42 } return f');
assertEq('byteLength' in this, false);
assertAsmLinkFail(m, this);
this['byteLength'] = null;
assertAsmLinkFail(m, this);
this['byteLength'] = {};
assertAsmLinkFail(m, this);
this['byteLength'] = function(){}
assertAsmLinkFail(m, this);
this['byteLength'] = (function(){}).bind(null);
assertAsmLinkFail(m, this);
this['byteLength'] = Function.prototype.call.bind();
assertAsmLinkFail(m, this);
this['byteLength'] = Function.prototype.call.bind({});
assertAsmLinkFail(m, this);
this['byteLength'] = Function.prototype.call.bind(function f() {});
assertAsmLinkFail(m, this);
this['byteLength'] = Function.prototype.call.bind(Math.sin);
assertAsmLinkFail(m, this);
this['byteLength'] =
  Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
assertEq(asmLink(m, this)(), 42);

var m = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var b1=glob.byteLength, b2=glob.byteLength; function f() { return 43 } return f');
assertEq(asmLink(m, this)(), 43);

// Tests for validation of change-heap function

const BYTELENGTH_IMPORT = "var len = glob.byteLength; ";
const IMPORT0 = BYTELENGTH_IMPORT;
const IMPORT1 = "var I8=glob.Int8Array; var i8=new I8(b); " + BYTELENGTH_IMPORT;
const IMPORT2 = "var I8=glob.Int8Array; var i8=new I8(b); var I32=glob.Int32Array; var i32=new I32(b); var II32=glob.Int32Array; " + BYTELENGTH_IMPORT;

       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function f() { return 42 } function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function b(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function f(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2=1) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2,xyz) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(...r) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2,...r) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch({b2}) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { ;if((len((b2))) & (0xffffff) || (len((b2)) <= (0xffffff)) || len(b2) > 0x80000000) {;;return false;;} ; i8=new I8(b2);; b=b2;; return true;; } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function ch2(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { 3; if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { b2=b2|0; if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(1) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(1 || 1) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(1 || 1 || 1) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(1 || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(1 & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || 1 || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(i8(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(xyz) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff && len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) | 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) == 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xfffffe || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0x1ffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) < 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xfffffe || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0x1000000 || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || 1) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) < 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || 1 > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0.0) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0xffffff) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x1000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffffff || len(b2) > 0x1000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0x1000000 || len(b2) > 0x1000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0x1000000 || len(b2) > 0x1000001) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000001) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) ; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) {} i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) {return false} i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return true; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT0 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i7=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; b=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=1; b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new 1; b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I7(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new b(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8; b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(1); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2,1); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); xyz=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=1; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; 1; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return 1 } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return false } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true; 1 } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i32=new I32(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i32=new I32(b2); i8=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); i32=new I32(b2); b=b2; return true } function f() { return 42 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I32(b2); i32=new I8(b2); b=b2; return true } function f() { return 42 } return f');
       asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); i32=new II32(b2); b=b2; return true } function f() { return 42 } return f');

// Tests for no calls in heap index expressions

const CHANGE_FUN = 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i8=new I8(b2); i32=new I32(b2); b=b2; return true }';
const SETUP = USE_ASM + IMPORT2 + 'var imul=glob.Math.imul; var ffi=ffis.ffi;' + CHANGE_FUN;

       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { i32[0] } return f');
       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { i32[0] = 0 } return f');
       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] } return f');
       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] = 0 } return f');
       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[(imul(i,i)|0) >> 2] = 0 } return f');
       asmCompile('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] = (imul(i,i)|0) } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[(ffi()|0) >> 2] } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[(g()|0) >> 2] } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[(TBL[i&0]()|0) >> 2] } function g() { return 0 } var TBL=[g]; return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[(g()|0) >> 2] = 0 } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] = g()|0 } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i32[(g()|0)>>2] >> 2] } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i32[(g()|0)>>2] >> 2] = 0 } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] = i32[(g()|0)>>2] } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[((i32[i>>2]|0) + (g()|0)) >> 2] } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[((i32[i>>2]|0) + (g()|0)) >> 2] = 0 } function g() { return 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', SETUP + 'function f() { var i = 0; i32[i >> 2] = (i32[i>>2]|0) + (g()|0) } function g() { return 0 } return f');
if (isSimdAvailable() && typeof SIMD !== 'undefined')
    asmCompile('glob', 'ffis', 'b', USE_ASM + IMPORT2 + 'var i4 = glob.SIMD.int32x4; var add = i4.add;' + CHANGE_FUN + 'function f(i) { i=i|0; i32[i4(i,1,2,i).x >> 2]; i32[add(i4(0,0,0,0),i4(1,1,1,1)).x >> 2]; } return f');

// Tests for constant heap accesses when change-heap is used

const HEADER = USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= MIN || len(b2) > 0x80000000) return false; i8=new I8(b2); b=b2; return true } ';
assertAsmTypeFail('glob', 'ffis', 'b', HEADER.replace('MIN', '0xffffff') + 'function f() { i8[0x1000000] = 0 } return f');
       asmCompile('glob', 'ffis', 'b', HEADER.replace('MIN', '0xffffff') + 'function f() { i8[0xffffff] = 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', HEADER.replace('MIN', '0x1000000') + 'function f() { i8[0x1000001] = 0 } return f');
       asmCompile('glob', 'ffis', 'b', HEADER.replace('MIN', '0x1000000') + 'function f() { i8[0x1000000] = 0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', HEADER.replace('MIN', '0xffffff') + 'function f() { return i8[0x1000000]|0 } return f');
       asmCompile('glob', 'ffis', 'b', HEADER.replace('MIN', '0xffffff') + 'function f() { return i8[0xffffff]|0 } return f');
assertAsmTypeFail('glob', 'ffis', 'b', HEADER.replace('MIN', '0x1000000') + 'function f() { return i8[0x1000001]|0 } return f');
       asmCompile('glob', 'ffis', 'b', HEADER.replace('MIN', '0x1000000') + 'function f() { return i8[0x1000000]|0 } return f');

// Tests for validation of heap length

var body = USE_ASM + IMPORT1 + 'function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0x1ffffff || len(b2) > 0x4000000) return false; i8=new I8(b2); b=b2; return true } function f() { return 42 } return ch';
var m = asmCompile('glob', 'ffis', 'b', body);
assertAsmLinkFail(m, this, null, new ArrayBuffer(BUF_CHANGE_MIN));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x1000000));
var changeHeap = asmLink(m, this, null, new ArrayBuffer(0x2000000));
assertEq(changeHeap(new ArrayBuffer(0x1000000)), false);
assertEq(changeHeap(new ArrayBuffer(0x2000000)), true);
assertEq(changeHeap(new ArrayBuffer(0x2000001)), false);
assertEq(changeHeap(new ArrayBuffer(0x4000000)), true);
assertEq(changeHeap(new ArrayBuffer(0x5000000)), false);
assertThrowsInstanceOf(() => changeHeap(null), TypeError);
assertThrowsInstanceOf(() => changeHeap({}), TypeError);
assertThrowsInstanceOf(() => changeHeap(new Int32Array(100)), TypeError);

var detached = new ArrayBuffer(BUF_CHANGE_MIN);
neuter(detached, "change-data");
assertEq(changeHeap(detached), false);

// Tests for runtime changing heap

const CHANGE_HEAP = 'var changeHeap = glob.byteLength;';

var changeHeapSource = `function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i32=new I32(b2); b=b2; return true }`;
var body = `var I32=glob.Int32Array; var i32=new I32(b);
            var len=glob.byteLength;` +
            changeHeapSource +
           `function get(i) { i=i|0; return i32[i>>2]|0 }
            function set(i, v) { i=i|0; v=v|0; i32[i>>2] = v }
            return {get:get, set:set, changeHeap:ch}`;

var m = asmCompile('glob', 'ffis', 'b', USE_ASM + body);
var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
var {get, set, changeHeap} = asmLink(m, this, null, buf1);

assertEq(m.toString(), "function anonymous(glob, ffis, b) {\n" + USE_ASM + body + "\n}");
assertEq(m.toSource(), "(function anonymous(glob, ffis, b) {\n" + USE_ASM + body + "\n})");
assertEq(changeHeap.toString(), changeHeapSource);
assertEq(changeHeap.toSource(), changeHeapSource);

set(0, 42);
set(4, 13);
set(4, 13);
assertEq(get(0), 42);
assertEq(get(4), 13);
set(BUF_CHANGE_MIN, 262);
assertEq(get(BUF_CHANGE_MIN), 0);
var buf2 = new ArrayBuffer(2*BUF_CHANGE_MIN);
assertEq(changeHeap(buf2), true);
assertEq(get(0), 0);
assertEq(get(4), 0);
set(BUF_CHANGE_MIN, 262);
assertEq(get(BUF_CHANGE_MIN), 262);
set(2*BUF_CHANGE_MIN, 262);
assertEq(get(2*BUF_CHANGE_MIN), 0);
changeHeap(buf1);
assertEq(get(0), 42);
assertEq(get(4), 13);
set(BUF_CHANGE_MIN, 262);
assertEq(get(BUF_CHANGE_MIN), 0);

if (ArrayBuffer.transfer) {
    var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
    var {get, set, changeHeap} = asmLink(m, this, null, buf1);
    set(0, 100);
    set(BUF_CHANGE_MIN - 4, 101);
    set(BUF_CHANGE_MIN, 102);
    var buf2 = ArrayBuffer.transfer(buf1);
    assertEq(changeHeap(buf2), true);
    assertEq(buf1.byteLength, 0);
    assertEq(buf2.byteLength, BUF_CHANGE_MIN);
    assertEq(get(0), 100);
    assertEq(get(BUF_CHANGE_MIN-4), 101);
    assertEq(get(BUF_CHANGE_MIN), 0);
    assertEq(get(2*BUF_CHANGE_MIN-4), 0);
    var buf3 = ArrayBuffer.transfer(buf2, 3*BUF_CHANGE_MIN);
    assertEq(changeHeap(buf3), true);
    assertEq(buf2.byteLength, 0);
    assertEq(buf3.byteLength, 3*BUF_CHANGE_MIN);
    assertEq(get(0), 100);
    assertEq(get(BUF_CHANGE_MIN-4), 101);
    assertEq(get(BUF_CHANGE_MIN), 0);
    assertEq(get(2*BUF_CHANGE_MIN), 0);
    set(BUF_CHANGE_MIN, 102);
    set(2*BUF_CHANGE_MIN, 103);
    assertEq(get(BUF_CHANGE_MIN), 102);
    assertEq(get(2*BUF_CHANGE_MIN), 103);
    var buf4 = ArrayBuffer.transfer(buf3, 2*BUF_CHANGE_MIN);
    assertEq(changeHeap(buf4), true);
    assertEq(buf3.byteLength, 0);
    assertEq(buf4.byteLength, 2*BUF_CHANGE_MIN);
    assertEq(get(0), 100);
    assertEq(get(BUF_CHANGE_MIN-4), 101);
    assertEq(get(BUF_CHANGE_MIN), 102);
    assertEq(get(2*BUF_CHANGE_MIN), 0);
    var buf5 = ArrayBuffer.transfer(buf4, 3*BUF_CHANGE_MIN);
    assertEq(changeHeap(buf5), true);
    assertEq(buf4.byteLength, 0);
    assertEq(buf5.byteLength, 3*BUF_CHANGE_MIN);
    assertEq(get(0), 100);
    assertEq(get(BUF_CHANGE_MIN-4), 101);
    assertEq(get(BUF_CHANGE_MIN), 102);
    assertEq(get(2*BUF_CHANGE_MIN), 0);
    var buf6 = ArrayBuffer.transfer(buf5, 0);
    assertEq(buf5.byteLength, 0);
    assertEq(buf6.byteLength, 0);
    assertEq(changeHeap(buf6), false);
}

var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
var buf2 = new ArrayBuffer(BUF_CHANGE_MIN);
var m = asmCompile('glob', 'ffis', 'b', USE_ASM +
                   `var len=glob.byteLength;
                    function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; b=b2; return true }
                    return ch`);
var changeHeap = asmLink(m, this, null, buf1);
assertEq(changeHeap(buf2), true);
neuter(buf2, "change-data");
assertEq(changeHeap(buf1), true);
neuter(buf1, "change-data");

var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
new Int32Array(buf1)[0] = 13;
var buf2 = new ArrayBuffer(BUF_CHANGE_MIN);
new Int32Array(buf2)[0] = 42;

// Tests for changing heap during an FFI:

// Set the warmup to '2' so we can hit both interp and ion FFI exits
setJitCompilerOption("ion.warmup.trigger", 2);
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);

var changeToBuf = null;
var m = asmCompile('glob', 'ffis', 'b', USE_ASM +
                   `var ffi=ffis.ffi;
                    var I32=glob.Int32Array; var i32=new I32(b);
                    var len=glob.byteLength;
                    function ch(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i32=new I32(b2); b=b2; return true }
                    function test(i) { i=i|0; var sum=0; sum = i32[i>>2]|0; sum = (sum + (ffi()|0))|0; sum = (sum + (i32[i>>2]|0))|0; return sum|0 }
                    return {test:test, changeHeap:ch}`);
var ffi = function() { changeHeap(changeToBuf); return 1 }
var {test, changeHeap} = asmLink(m, this, {ffi:ffi}, buf1);
changeToBuf = buf1;
assertEq(test(0), 27);
changeToBuf = buf2;
assertEq(test(0), 56);
changeToBuf = buf2;
assertEq(test(0), 85);
changeToBuf = buf1;
assertEq(test(0), 56);
changeToBuf = buf1;
assertEq(test(0), 27);

var ffi = function() { return { valueOf:function() { changeHeap(changeToBuf); return 100 } } };
var {test, changeHeap} = asmLink(m, this, {ffi:ffi}, buf1);
changeToBuf = buf1;
assertEq(test(0), 126);
changeToBuf = buf2;
assertEq(test(0), 155);
changeToBuf = buf2;
assertEq(test(0), 184);
changeToBuf = buf1;
assertEq(test(0), 155);
changeToBuf = buf1;
assertEq(test(0), 126);

if (ArrayBuffer.transfer) {
    var buf = new ArrayBuffer(BUF_CHANGE_MIN);
    new Int32Array(buf)[0] = 3;
    var ffi = function() {
        var buf2 = ArrayBuffer.transfer(buf, 2*BUF_CHANGE_MIN);
        new Int32Array(buf2)[BUF_CHANGE_MIN/4] = 13;
        assertEq(changeHeap(buf2), true);
        return 1
    }
    var {test, changeHeap} = asmLink(m, this, {ffi:ffi}, buf);
    assertEq(test(BUF_CHANGE_MIN), 14);
}
