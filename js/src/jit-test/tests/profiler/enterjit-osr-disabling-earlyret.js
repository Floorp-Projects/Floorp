setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

enableSPSProfilingWithSlowAssertions();
(function() {
   var n = 50;
   while (n--) {
       disableSPSProfiling();
       if (!n)
	   return;
       enableSPSProfilingWithSlowAssertions();
   }
})();
