// |jit-test| module

if (typeof oomTest !== "function")
    quit();

oomTest(() => import.meta);
