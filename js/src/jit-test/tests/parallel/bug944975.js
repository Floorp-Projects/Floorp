if (getBuildConfiguration().parallelJS) {
  var map_toSource_called = false;
  var mapPar_toSource_called = false;

  Array.prototype.mapPar.toSource = function() {
    mapPar_toSource_called = true;
  };

  Array.prototype.map.toSource = function() {
    map_toSource_called = true;
  };

  try { new Array.prototype.mapPar; } catch (e) {}
  try { new Array.prototype.map; } catch (e) {}

  assertEq(map_toSource_called, mapPar_toSource_called);
}
