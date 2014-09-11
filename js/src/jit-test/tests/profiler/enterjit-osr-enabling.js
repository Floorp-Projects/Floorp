setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

(function() {
   enableSPSProfilingWithSlowAssertions();
   var n = 50;
   while (n--);
})();
