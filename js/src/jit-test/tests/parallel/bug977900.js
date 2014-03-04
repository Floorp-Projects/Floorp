if (!getBuildConfiguration().parallelJS)
    quit();

function chooseMax(a, b) { return a>b?a:b;};
var a = [1,2].scatterPar([0,0], -1, chooseMax);
