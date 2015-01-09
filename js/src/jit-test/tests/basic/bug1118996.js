// |jit-test| allow-oom
g = newGlobal();
gcparam('maxBytes', gcparam('gcBytes'));
evaluate("return 0", ({
    global: g,
    newContext: true
}));
