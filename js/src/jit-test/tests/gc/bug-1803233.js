// |jit-test| allow-oom; allow-unhandlable-oom

gczeal(10, 1);
gcparam("maxBytes", gcparam("gcBytes"));
newGlobal();
