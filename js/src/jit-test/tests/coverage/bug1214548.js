if (!('oomTest' in this))
    quit();

oomTest(() => {
  var g = newGlobal();
  g.eval("\
    function f(){}; \
    getLcovInfo(); \
  ");
});
