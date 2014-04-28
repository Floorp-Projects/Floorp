setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

(function() {
   var n = 50;
   while (n--) {
       enableSPSProfilingWithSlowAssertions();
       if (!n)
	   return;
       disableSPSProfiling();
   }
})();
