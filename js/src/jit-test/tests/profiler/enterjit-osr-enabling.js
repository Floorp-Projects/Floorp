setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

(function() {
   enableGeckoProfilingWithSlowAssertions();
   var n = 50;
   while (n--);
})();
