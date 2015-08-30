function prepareEnv(cb) {
  SpecialPowers.pushPrefEnv({"set":[["dom.mozApps.debug", true]]}, cb);
}
