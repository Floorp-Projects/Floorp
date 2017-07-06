// |jit-test| allow-unhandlable-oom
gczeal(7,1);
try {
gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
newGlobal("same-compartment");
} catch(exc1) {}
gczeal(1);
