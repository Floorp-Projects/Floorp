setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

(function() {
   var n = 50;
   while (n--) {
       enableGeckoProfilingWithSlowAssertions();
       if (!n)
	   return;
       disableGeckoProfiling();
   }
})();
