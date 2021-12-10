// |jit-test| skip-if: helperThreadCount() === 0

var fe = "vv";
for (i = 0; i < 24; i++) fe += fe;
offThreadCompileToStencil(fe, {});
