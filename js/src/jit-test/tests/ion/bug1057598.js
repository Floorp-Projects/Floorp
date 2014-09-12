setObjectMetadataCallback(function( r, ... d)  {});
setJitCompilerOption("ion.warmup.trigger", 20);
var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
}
var uceFault_str_split = eval(uneval(uceFault).replace('uceFault', 'uceFault_str_split'))
function rstr_split(i) {
    var x = "str01234567899876543210rts".split("" + i);
    if (uceFault_str_split(i) || uceFault_str_split(i)) {
    }
}
for (i = 0; i < 100; i++) {
    rstr_split(i);
}
