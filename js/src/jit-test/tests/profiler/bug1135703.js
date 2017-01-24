
for (var idx = 0; idx < 20; ++idx) {
  newFunc("enableGeckoProfilingWithSlowAssertions(); disableGeckoProfiling();");
}
newFunc("enableGeckoProfiling();");
function newFunc(x) { new Function(x)(); };
