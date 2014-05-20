if (getBuildConfiguration().parallelJS) {
    for (var a = 0; a < 2000; a++) {
        Array.buildPar(500, (function() {
            return {}
        }))
    }
}
