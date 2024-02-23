function allocateSomeStuff() {
  return {a: "a fish", b: [1, 2, 3]};
}

oomTest(() => {
  // Run a minor GC with a small nursery.
  gcparam('minNurseryBytes', 256 * 1024);
  gcparam('maxNurseryBytes', 256 * 1024);
  allocateSomeStuff();
  minorgc();

  // Run a minor GC with a larger nursery to get it to attempt to grow and
  // fail the allocation there.
  gcparam('maxNurseryBytes', 1024 * 1024);
  gcparam('minNurseryBytes', 1024 * 1024);
  allocateSomeStuff();
  minorgc();
});

