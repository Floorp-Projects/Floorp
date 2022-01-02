function asmJSGetter() {
    "use asm";
    return {}
}
var sym = Symbol();
var o = {};
o.__defineGetter__(sym, asmJSGetter);

for (var i = 0; i < 10; i++) {
    o[sym];
}
