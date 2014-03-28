setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

enableSPSProfilingAssertions(true);
(function() {
   var n = 50;
   while (n--);
})();
