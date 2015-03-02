// |jit-test| error: out of memory

gcparam("maxBytes", gcparam("gcBytes"));
eval(`
    gczeal(2, 1);
    newGlobal();
`);
