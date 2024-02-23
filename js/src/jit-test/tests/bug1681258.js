// |jit-test| --fast-warmup;--blinterp-warmup-threshold=10
ignoreUnhandledRejections();

oomTest(async function() {
    let m = parseModule(``);
    moduleLink(m);
    await moduleEvaluate(m);
});
