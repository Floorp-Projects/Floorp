Object.defineProperty(this, "fuzzutils", {
      value: {
          orig_evaluate: evaluate,
          evaluate: function(c, o) {
              if (!o) {
                  o = {};
              }
              o.catchTermination = true;
              return fuzzutils.orig_evaluate(c, o);
          },
      }
  });
  gczeal(21, 10);
  fuzzutils.evaluate(`
enableShellAllocationMetadataBuilder();
function test() {}
`);
