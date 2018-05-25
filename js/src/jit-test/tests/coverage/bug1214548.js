if (!('oomTest' in this))
    quit();

oomTest(() => {
  var g = newGlobal({sameZoneAs: this});
  g.eval("\
    function f(){}; \
    getLcovInfo(); \
  ");
});
