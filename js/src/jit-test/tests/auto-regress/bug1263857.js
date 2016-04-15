// |jit-test| allow-oom; allow-unhandlable-oom
gcparam("maxBytes", gcparam("gcBytes") + 1);
fullcompartmentchecks(true);
/x/g[Symbol.replace]("        x".repeat(32768), "");
