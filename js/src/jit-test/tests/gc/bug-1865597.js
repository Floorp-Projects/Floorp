// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
  gcparam('parallelMarkingEnabled', false);
  assertEq(gcparam('parallelMarkingEnabled'), 0);
  gcparam('parallelMarkingEnabled', true);
  assertEq(gcparam('parallelMarkingEnabled'), 1);
  gcparam('parallelMarkingEnabled', false);
  assertEq(gcparam('parallelMarkingEnabled'), 0);
});
