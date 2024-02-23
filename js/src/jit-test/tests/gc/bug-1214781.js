// |jit-test| allow-oom

try {
    gcparam("maxBytes", gcparam("gcBytes"));
    newGlobal("");
} catch (e) {};
oomTest(function() {})
