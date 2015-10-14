oomTest(() => {
  var g = newGlobal();
  g.eval("\
    function f(){}; \
    getLcovInfo(); \
  ");
});
