// Check byte counts produced by takeCensus.

const g = newGlobal();
g.eval("setLazyParsingDisabled(true)");

const dbg = new Debugger(g);

g.evaluate("function one() {}", { fileName: "one.js" });
g.evaluate(`function two1() {}
            function two2() {}`,
           { fileName: "two.js" });
g.evaluate(`function three1() {}
            function three2() {}
            function three3() {}`,
           { fileName: "three.js" });

const report = dbg.memory.takeCensus({
  breakdown: {
    by: "coarseType",
    scripts: {
      by: "filename",
      then: { by: "count", count: true, bytes: false },
      noFilename: {
        by: "internalType",
        then: { by: "count", count: true, bytes: false }
      }
    },

    // Not really interested in these, but they're here for completeness.
    objects: { by: "count", count: true, byte: false },
    strings: { by: "count", count: true, byte: false },
    other:   { by: "count", count: true, byte: false },
  }
});

print(JSON.stringify(report, null, 4));

assertEq(report.scripts["one.js"].count, 1);
assertEq(report.scripts["two.js"].count, 2);
assertEq(report.scripts["three.js"].count, 3);

const noFilename = report.scripts.noFilename;
assertEq(!!noFilename, true);
assertEq(typeof noFilename, "object");
