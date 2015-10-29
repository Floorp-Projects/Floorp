// |jit-test| error: ReferenceError
if (!('oomTest' in this))
    a;

enableShellObjectMetadataCallback()
oomTest(() => {
  newGlobal()
})
gczeal(9, 1);
a;
