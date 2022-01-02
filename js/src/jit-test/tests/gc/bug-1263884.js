// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
  eval(`
    var argObj = function () { return arguments }()
    for (var p in argObj);
    delete argObj.callee;
  `);
});
