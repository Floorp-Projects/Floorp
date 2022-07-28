// Assert that we get a useful amount of memory for memory64 on tier-1 64-bit
// systems with our primary compilers.

var conf = getBuildConfiguration();

// Only 64-bit systems
if (conf["pointer-byte-size"] != 8) {
    quit(0);
}

// MIPS64 tops out below 2GB
if (conf["mips64"]) {
    quit(0);
}

var eightGB = 8 * 1024 * 1024 * 1024;
var pageSize = 65536;
assertEq(wasmMaxMemoryPages("i64") >= eightGB / pageSize, true);
