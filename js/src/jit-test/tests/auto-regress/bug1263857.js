// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: getBuildConfiguration()['android']
// Disabled on Android due to harness problems (Bug 1532654)
gcparam("maxBytes", gcparam("gcBytes") + 1);
fullcompartmentchecks(true);
/x/g[Symbol.replace]("        x".repeat(32768), "");
