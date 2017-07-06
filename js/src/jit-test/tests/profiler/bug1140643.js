// |jit-test| allow-oom; allow-unhandlable-oom
enableGeckoProfiling();
loadFile('\
for (var i = 0; i < 2; i++) {\
    obj = { m: function () {} };\
    obj.watch("m", function () { float32 = 0 + obj.foo; });\
    obj.m = 0;\
}\
');
gcparam("maxBytes", gcparam("gcBytes") + (1)*1024);
newGlobal("same-compartment");
function loadFile(lfVarx) {
  evaluate(lfVarx, { noScriptRval : true, isRunOnce : true }); 
}
