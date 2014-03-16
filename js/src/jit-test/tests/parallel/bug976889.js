if (!getBuildConfiguration().parallelJS)
  quit(0);

Array.buildPar(5, function() {
    return [].t = encodeURI
})
