
setJitCompilerOption("offthread-compilation.enable", 0);
setJitCompilerOption("ion.warmup.trigger", 100);

Array.prototype.__proto__ = function() {};
var LENGTH = 1024;
var big = [];
for (var i = 0; i < LENGTH; i++)
  big[i] = i;
