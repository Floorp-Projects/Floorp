
du = new Debugger();
if (typeof du.setupTraceLogger === "function" &&
    typeof oomTest === 'function')
{
    du.setupTraceLogger({Scripts: true});
    for (var idx = 0; idx < 1; idx++) {
      oomTest(function() {
          m = parseModule("x");
          m.declarationInstantiation();
          m.evaluation();
      })
    }
}
