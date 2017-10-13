// |jit-test| allow-oom; allow-unhandlable-oom

gcparam("maxBytes", gcparam("gcBytes"));
eval(`
    gczeal(2, 1);
    newGlobal();
`);
