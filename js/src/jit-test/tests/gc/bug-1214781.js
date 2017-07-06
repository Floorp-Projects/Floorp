// |jit-test| allow-unhandlable-oom
if (!('oomTest' in this))
    quit();

try {
    gcparam("maxBytes", gcparam("gcBytes"));
    newGlobal("");
} catch (e) {};
oomTest(function() {})
