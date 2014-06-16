// Stale assertion in initDenseElementsUnbarriered, but it's a useful test anyhow.

if (!getBuildConfiguration().parallelJS)
  quit(0);

var sum=0;
for ( var i=0 ; i < 1000 ; i++ ) {
    sum += ([0].mapPar(function (...xs) { return xs.length; }))[0];
}
assertEq(sum, 3000);		// Function is invoked with value, index, self
