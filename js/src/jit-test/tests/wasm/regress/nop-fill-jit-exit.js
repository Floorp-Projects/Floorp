// |jit-test| --arm-asm-nop-fill=1
//
try {
    enableSingleStepProfiling();
    disableSingleStepProfiling();
} catch (e) {
    // Early quit on platforms not supporting single step profiling.
    quit();
}

load(libdir + "asm.js");

var ffi = function(enable) {
    enableGeckoProfiling();
    enableSingleStepProfiling();
}
var f = asmLink(asmCompile('global', 'ffis',
  USE_ASM + `
    var ffi=ffis.ffi;
    function f(i) {
      i=i|0;
      ffi(i|0);
    } return f
  `), null, {
    ffi
});
f(0);
f(+1);
