if (getBuildConfiguration().parallelJS && typeof Symbol === "function") {
  x = Array.buildPar(7, Symbol);
  Array.prototype.push.call(x, 2);
  x.mapPar(function(){}, 1)
}
