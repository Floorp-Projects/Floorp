if (getBuildConfiguration().parallelJS) {
    Array.buildPar(6763, function() {
        return Math.fround(Math.round(Math.fround(-0)));
    });
}
