setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

enableSPSProfilingWithSlowAssertions();
(function() {
   disableSPSProfiling();
   var n = 50;
   while (n--);
})();
