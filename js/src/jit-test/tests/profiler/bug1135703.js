
for (var idx = 0; idx < 20; ++idx) {
  newFunc("enableSPSProfilingWithSlowAssertions(); disableSPSProfiling();");
}
newFunc("enableSPSProfiling();");
function newFunc(x) { new Function(x)(); };
