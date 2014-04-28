setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

(function() {
   enableSPSProfilingWithSlowAssertions();
   var n = 50;
   while (n--);
})();
