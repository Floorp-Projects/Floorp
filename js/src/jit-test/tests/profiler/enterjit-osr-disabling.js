setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

enableSPSProfilingWithSlowAssertions();
(function() {
   disableSPSProfiling();
   var n = 50;
   while (n--);
})();
