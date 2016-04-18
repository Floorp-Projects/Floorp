if (!('oomTest' in this))
    quit();

oomTest(function() {
  eval(`
    var argObj = function () { return arguments }()
    for (var p in argObj);
    delete argObj.callee;
  `);
});
