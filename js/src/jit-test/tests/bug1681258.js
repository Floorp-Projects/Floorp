// |jit-test| skip-if: !('oomTest' in this);--enable-top-level-await;--fast-warmup;--blinterp-warmup-threshold=10
ignoreUnhandledRejections();

oomTest(async function() {
    let m = parseModule(``);
    m.declarationInstantiation();
    await m.evaluation();
});
