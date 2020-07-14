// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
  new AggregateError([]);
});
