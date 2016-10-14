var lfLogBuffer = `
  gczeal(14);
  enableSPSProfiling();
  gczeal(15,3);
  var s = "";
  for (let i = 0; i != 30; i+=2) {}
  readSPSProfilingStack(s, "c0d1c0d1c0d1c0d1c0d1c0d1c0d1c0");
`;
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
  evaluate(lfVarx);
}
