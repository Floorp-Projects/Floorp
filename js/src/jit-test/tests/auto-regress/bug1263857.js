// |jit-test| allow-oom; allow-unhandlable-oom; skip-if: getBuildConfiguration('arm64')
//
// Disabled on ARM64 due to Bug 1529034: the harness expects unhandlable-oom error
// messages on stderr, but Android builds output to the system log file.
// Therefore unhandlable-oom appears as a segfault with no message, and the test fails.
gcparam("maxBytes", gcparam("gcBytes") + 1);
fullcompartmentchecks(true);
/x/g[Symbol.replace]("        x".repeat(32768), "");
