// |jit-test| allow-unhandlable-oom; skip-if: !('oomTest' in this)

const otherGlobalNewCompartment = newGlobal({newCompartment: true});

oomTest(function() {
  let {transplant} = transplantableObject();
  transplant(otherGlobalNewCompartment);
});
